/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ft8_encoder.h"
#include "message.h"
#include <bitset>
#include <boost/multiprecision/cpp_int.hpp>
#include <regex>
#include <sstream>
#include <string_view>
#include <gmpxx.h>
#include <fstream>
#include <cmath>

ft8_encoder::ft8_encoder () : d_logger ("FT8_encoding")
{
  d_logger.info ("Message encoding created");
}
ft8_encoder::ft8_encoder (const message &message) : d_logger ("FT8_encoding")
{
  d_logger.info ("FT8 encoding object constructed");
  bitfields (message);
}

void
ft8_encoder::bitfields (const message &message)
{
  message::message_type type = message.message_type_detection ();

  switch (type)
    {
    case message::message_type::standard: // or euvhf
      encode_standard (message);
      break;
    /*case message::message_type::dxpedition:
      encode_dexpedition (message);
      break;
    case message::message_type::field_day:
      encode_field_day (message);
      break;
    case message::message_type::field_dayx:
      encode_field_dayx (message);
      break;
    case message::message_type::telemetry:
      encode_telemetry (message);
      break;
    case message::message_type::rtty_ru:
      encode_rtty_ru (message);
      break;
    case message::message_type::nonstd_call:
      encode_nonstd_call (message);
      break;
    case message::message_type::euvhfx:
      encode_euvhfx (message);
      break;
    case message::message_type::euvhf:
      encode_standard (message);
      break;
    case message::message_type::unknown:
      d_logger.error ("no message");
      break;
    case message::message_type::free_text:
      encode_free_text (message);
      break;*/
    }
}

std::vector<int> 
ft8_encoder::bits_to_fsk8(const std::bitset<174>& ldpc_bits)
{
    //from table in docs
    const int gray_map[8] = {0 /*000*/, 1/*001*/, 3/*010*/, 2/*011*/, 7/*111*/, 6/*110*/, 4/*100*/, 5/*101*/}; 
    std::vector<int> symbols;
    symbols.reserve(58); // (174/3 = 58)

    //process bits in 3
    for (int i = 0; i < 174; i += 3){
        int bit_trio = 0;
        // converts to 0-7 range
        bit_trio |= (ldpc_bits[i] ? 1 : 0) <<2; //ie. 0000 to 0100 = 4
        bit_trio |= (ldpc_bits[i+1] ? 1 : 0) <<1;
        bit_trio |= (ldpc_bits[i+2] ? 1 : 0);

        int channel_symbol = gray_map[bit_trio];
        symbols.push_back(channel_symbol);
    }

    //array from docs
    const std::vector<int> S = {3,1,4,0,6,5,2}; //sync sequence, consta's array
    const std::vector<int> Ma(symbols.begin(), symbols.begin() + 29);
    const std::vector<int> Mb(symbols.begin() + 29, symbols.end());


    //transmission sequence (from docs): S + Ma + S + Mb + S
    std::vector<int> transmit_symbols;
    transmit_symbols.reserve(79); //3 + 29 + 3 + 39 + 3 = 79

    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());
    transmit_symbols.insert(transmit_symbols.end(), Ma.begin(), Ma.end());
    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());
    transmit_symbols.insert(transmit_symbols.end(), Mb.begin(), Mb.end());
    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());
    
    /*std::vector<bool> result;
    result.reserve(transmit_symbols.size() * 3);

    for (int symbol: transmit_symbols) {
        result.push_back((symbol >> 2) & 1); 
        result.push_back((symbol >> 1) & 1);
        result.push_back(symbol & 1); 
    }

    return result;*/

    return transmit_symbols;
}

std::bitset<174>
ft8_encoder::apply_ldpc(const std::bitset<91>& crc_bits)
{
    auto generator = load_generator_matrix("generator.dat");
    std::bitset<174> complete_msg;

    for (int i = 0; i<91; i++){
        complete_msg[i] = crc_bits[i];
    }
    
    for (int parity_bit = 0; parity_bit <83; parity_bit++){
        bool parity_val = 0;
        for (int i = 0; i < 91; i++) {
            if (generator[parity_bit][i] && crc_bits[i]) {
                parity_val = !parity_val;
            }
        }
        complete_msg[91 + parity_bit] = parity_val;
    }
    return complete_msg;
}

std::vector<std::bitset<91>> 
ft8_encoder::load_generator_matrix(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        d_logger.error("Cannot open file for generator: {} ", filename);
    }

    std::vector<std::bitset<91>> generator_matrix;
    generator_matrix.reserve(83);
    
    std::string line;
    int row = 0;

    while (std::getline(file, line) && row < 83) {
        if (line.empty()) continue;
        if (line.length() >= 91) {
            bool is_binary_line = true;
            for (int i = 0; i < 91 && is_binary_line; i++) {
                if (line[i] != '0' && line[i] != '1') {
                    is_binary_line = false;
                }
            }
            
            if (is_binary_line) {
                std::bitset<91> m_row;
                for (int col = 0; col < 91; col++) {
                    m_row[col] = (line[col] == '1');
                }
                generator_matrix.push_back(m_row);
                row++;
            }
        }
    }

    if (row != 83) {
        d_logger.error("Invalid row count, expected 83 rows");
    }
    
    return generator_matrix;
}

std::bitset<91>
ft8_encoder::calc_crc(const std::bitset<77>& message_bits) 
{
    //basically a 14-bit shift register
    //if bits fall of the left edge
    //it trigger the polynomial correction
    //so these other bits in the register are flipped using the XOR
    //it's like a conveyer belt of bits 

    constexpr uint16_t crc_polynomial = 0x6757;
    constexpr uint16_t crc_start_val = 0x0000;
    constexpr uint16_t crc_14bit_mask = 0x3FFF; //00 + (1*14)
    constexpr uint16_t crc_msb_mask = 0x2000; //only bit 13 set
   
    uint16_t crc_register = crc_start_val;

    for (size_t i = 0; i<77; ++i){ //i = bit index
        bool input_bit = message_bits[76 - i]; //most significant bit order for ft8
        bool overflow_bit = (crc_register & crc_msb_mask) != 0; //is bit 13 set to 1 (check before shift left to bit 14 and fall of register)

        crc_register <<= 1;
        crc_register &= crc_14bit_mask; //keep only 14 bits

        if (input_bit) { //input bit to least significant position (right)
            crc_register  ^= 1;
        }
        
        if (overflow_bit) {
            crc_register ^= crc_polynomial;
        }
    }
    
    std::bitset<91> complete_msg;
    for (size_t i=0; i<77; ++i) {
        complete_msg[i] = message_bits[i];
    }
    for (size_t i=0; i<13; ++i) {
        complete_msg[77 + i] = (crc_register >> (13-i)) & 1; //move to msp + mask w/ 1
    }
    
    d_logger.info("91-bit FT8 msg with CRC14 created");
    return complete_msg;
}

void pack_bits(std::bitset<77>& bits, int& bit_pos, uint64_t val, int num_bits)
{
  for (int i = num_bits - 1; i >=0; --i) {
    bits[bit_pos++] = (val >> i) & 1;
  }
}

std::bitset<77>
ft8_encoder::encode_standard (const message &message)
{
  std::string temp_msg = message.get_message ();
  uint32_t c28a = 0, c28b = 0;
  bool r1 = 0, R1 = 0;
  uint16_t g15 = 0;
  uint8_t i3 = 1;
  
  //encode first call sign
  c28a = encode_28 (temp_msg, message);
  //encode second call sign
  c28b = encode_28 (temp_msg, message);
  g15 = g4_to_15 (temp_msg, message);
  r1 = encode_r1(temp_msg);
  R1 = encode_R1(temp_msg);

  std::bitset<77> message_bits;
  int bit_pos = 0;

  pack_bits(message_bits, bit_pos, c28a, 28);
  pack_bits(message_bits, bit_pos, c28b, 28);
  pack_bits(message_bits, bit_pos, r1, 1);
  pack_bits(message_bits, bit_pos, R1, 1);
  pack_bits(message_bits, bit_pos, g15, 15);
  pack_bits(message_bits, bit_pos, i3, 3);

  d_logger.info("77-bit ft8 assembed");
  
  //encode_ft8_complete(message_bits); 
  return message_bits;
}

uint32_t
ft8_encoder::encode_28 (std::string &temp_msg, const message &message)
{
  if (temp_msg.find ("DE ") != std::string::npos)
    {
      temp_msg
          = std::regex_replace (temp_msg, std::regex ("\\bDE\\b\\s*"), "");
      ;
      return 0;
    }
  if (temp_msg.find ("QRZ ") != std::string::npos)
    {
      temp_msg
          = std::regex_replace (temp_msg, std::regex ("\\bQRZ\\b\\s*"), "");
      return 1;
    }

  std::regex cq_only (R"(^CQ\s*$)");
  if (std::regex_match (temp_msg, cq_only))
    {
      temp_msg
          = std::regex_replace (temp_msg, std::regex ("\\bCQ\\b\\s*"), "");
      return 2;
    }

  std::regex cq_number (R"(CQ\s+(\d{1,3})(?=\s|$))");
  std::smatch number_match;
  if (std::regex_search (temp_msg, number_match, cq_number))
    {
      uint32_t number = std::stoul (number_match[1].str ());
      if (number <= 999)
        {
          temp_msg = std::regex_replace (temp_msg, cq_number, "");
          return 3 + number;
        }
    }

  std::regex cq_one_letter (R"(CQ\s+([A-Z])(?=\s|$))");
  std::smatch one_letter;
  if (std::regex_search (temp_msg, one_letter, cq_one_letter))
    {
      char letter = one_letter[1].str ()[0];
      temp_msg = std::regex_replace (temp_msg, cq_one_letter, "");
      return 1004 + (letter - 'A');
    }

  std::regex cq_two_letter (R"(CQ\s+([A-Z]{2})(?=\s|$))");
  std::smatch two_letter;
  if (std::regex_search (temp_msg, two_letter, cq_two_letter))
    {
      std::string letters = two_letter[1].str ();
      temp_msg = std::regex_replace (temp_msg, cq_two_letter, "");
      uint32_t letter_val = (letters[0] - 'A') * 26
                            + (letters[1] - 'A'); // essnetially base 26
      return 1031 + letter_val;
    }

  std::regex cq_three_letter (R"(CQ\s+([A-Z]{3})(?=\s|$))");
  std::smatch three_letter;
  if (std::regex_search (temp_msg, three_letter, cq_three_letter))
    {
      std::string letters = three_letter[1].str ();
      temp_msg = std::regex_replace (temp_msg, cq_three_letter, "");
      uint32_t letter_val = (letters[0] - 'A') * 26 * 26
                            + (letters[1] - 'A') * 26 + (letters[2] - 'A');
      return 1760 + letter_val;
    }

  std::regex cq_four_letter (R"(CQ\s+([A-Z]{4})(?=\s|$))");
  std::smatch four_letter;
  if (std::regex_search (temp_msg, four_letter, cq_four_letter))
    {
      std::string letters = four_letter[1].str ();
      temp_msg = std::regex_replace (temp_msg, cq_four_letter, "");
      uint32_t letter_val = (letters[0] - 'A') * 26 * 26 * 26
                            + (letters[1] - 'A') * 26 * 26
                            + (letters[2] - 'A') * 26 + (letters[3] - 'A');
      return 21443 + letter_val;
    }

  std::istringstream keywords (temp_msg);
  std::string keyword;
  while (keywords >> keyword)
    {
      if (message.is_callsign (keyword))
        {
          uint32_t encoded_28 = std_call_to_28 (temp_msg);
          size_t pos = temp_msg.find (keyword);
          if (pos != std::string::npos)
            {
              temp_msg.erase (pos, keyword.length ());
            }
          return encoded_28;
        }
    }

  return 0;
}

uint32_t
ft8_encoder::std_call_to_28 (std::string &msg)
{
  constexpr std::string_view a1
      = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // space, numbers, letters
  constexpr std::string_view a2
      = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // numbers, letters
  constexpr std::string_view a3 = "0123456789"; // numbers
  constexpr std::string_view a4
      = " ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // space, letters

  std::string temp_msg = msg;
  temp_msg.resize (6, ' '); // max size 6 chars

  size_t i1 = a1.find (temp_msg[0]);
  size_t i2 = a2.find (temp_msg[1]);
  size_t i3 = a3.find (temp_msg[2]);
  size_t i4 = a4.find (temp_msg[3]);
  size_t i5 = a4.find (temp_msg[4]);
  size_t i6 = a4.find (temp_msg[5]);

  constexpr size_t ntokens = 2063592;
  constexpr size_t max22 = 4194304;

  if (i1 == std::string_view::npos || i3 == std::string_view::npos
      || i3 == std::string_view::npos || i4 == std::string_view::npos
      || i5 == std::string_view::npos || i6 == std::string_view::npos)
    {
      d_logger.error (
          "Invalid character tyring to be encoded in standard message");
    }

  uint32_t n28 = ntokens + max22 + 36 * 10 * 27 * 27 * 27 * i1
                 + 10 * 27 * 27 * 27 * i2 + 27 * 27 * 27 * i3 + 27 * 27 * i4
                 + 27 * i5 + i6;

  return n28;
}

uint64_t
ft8_encoder::nonstd_to_58 (std::string &msg)
{
  constexpr std::string_view a = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";
  uint64_t n58 = 0;

  std::string temp_msg = msg;
  temp_msg.resize (11, ' '); // max size 11 chars

  for (char c : temp_msg)
    {
      size_t i = a.find (c);
      if (i == std::string_view::npos)
        {
          d_logger.error ("Invalid character for non-standard message");
        }
      n58 = n58 * a.length () + i;
    }

  return n58;
}

std::bitset<71>
ft8_encoder::free_text_to_f71 (std::string &msg)
{
  constexpr std::string_view a = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?";
  std::string temp_msg = msg;
  temp_msg.resize (13, ' ');

  mpz_class value = 0;

  for (char c : temp_msg)
    {
      size_t i = a.find (c);
      if (i == std::string_view::npos)
        {
          d_logger.error ("Invalid character for free text message");
          i = 0;
        }
      value = value * 42 + i;
    }

  std::bitset<71> bits;
  
  for (size_t i = 0; i<71; ++i){
    bits[i] = mpz_tstbit(value.get_mpz_t(), i);
  }
    
  return bits;
}

uint16_t
ft8_encoder::g4_to_15 (std::string &temp_msg, const message &message)
{ // in original protocol RRR, RR73, and 73 are also encoded here, removed
  // because it is redundant
  std::istringstream keywords (temp_msg);
  std::string keyword;

  while (keywords >> keyword)
    {
      if (message.is_grid_square (keyword))
        {
          uint16_t encoded_15 = (keyword[0] - 'A') * 18 * 10 * 10
                                + (keyword[1] - 'A') * 10 * 10
                                + (keyword[2] - '0') * 10 + (keyword[3] - '0');

          size_t pos = temp_msg.find (keyword);
          if (pos != std::string::npos)
            {
              temp_msg.erase (pos, keyword.length ());
            }
          return encoded_15;
        }
    }
  return 0;
}

uint32_t
ft8_encoder::g6_to_15 (const message &message)
{
  std::string temp_msg = message.get_message ();
  std::istringstream keywords (temp_msg);
  std::string keyword;

  while (keywords >> keyword)
    {
      if (message.is_grid_6square (keyword))
        {
          uint32_t encoded_25 = (keyword[0] - 'A') * 18 * 10 * 10 * 24 * 24
                                + (keyword[1] - 'A') * 10 * 10 * 24 * 24
                                + (keyword[2] - '0') * 10 * 24 * 24
                                + (keyword[3] - '0') * 24 * 24
                                + (keyword[4] - '0') * 24 + (keyword[5] - '0');

          size_t pos = temp_msg.find (keyword);
          if (pos != std::string::npos)
            {
              temp_msg.erase (pos, keyword.length ());
            }
          return encoded_25;
        }
    }
  return 0;
}

bool
ft8_encoder::encode_fdclass (char fd_class)
{
  if (fd_class >= 'A' && fd_class <= 'F')
    {
      return fd_class - 'A';
    }
  return 0;
}

bool
ft8_encoder::encode_p1 (std::string &msg)
{
  size_t pos = msg.find ("/P");
  if (pos != std::string::npos)
    {
      msg.erase (pos);
      return true;
    }
  return 0;
}

bool
ft8_encoder::encode_r1 (std::string &msg)
{
  size_t pos = msg.find ("/R");
  if (pos != std::string::npos)
    {
      msg.erase (pos);
      return true;
    }
  return 0;
}

bool
ft8_encoder::encode_R1(std::string &msg)
{
  std::regex r_one(R"(\bR\s+)");
  if (std::regex_search(msg, r_one))
    {
      msg = std::regex_replace(msg, r_one, "");
      return true;
    }
  return false;
}

bool
ft8_encoder::encode_t1 (std::string &msg)
{
  if (msg.substr (0, 3) == "TU")
    {
      msg.erase (0);
      return true;
    }
  return 0;
}

uint8_t
ft8_encoder::encode_r2 (std::string &msg)
{
  if (msg == "RRR")
    return 1;
  if (msg == "RR73")
    return 2;
  if (msg == "73")
    return 3;
  return 0;
}

uint8_t
ft8_encoder::encode_sigreport (std::string &msg)
{
  int db = std::stoi (msg);
  return (db + 30) / 2;
}


// signal reports are only allowed for even numbers,
// handle contest formats??

// std::vector<float> 
// ft8_encoder::encode_ft8_complete(std::bitset<77> message_bits)
// {
//   std::bitset<91> crc = calc_crc(message_bits);
//   std::bitset<174> ldpc = apply_ldpc(crc);
//   std::vector<int> symbols = bits_to_fsk8(ldpc);
//   std::vector<float> waveform = generate_ft8_waveform(symbols, sample_rate_const);
//   return waveform;
// }

// std::vector<float>
// ft8_encoder::generate_ft8_waveform(const std::vector<int>& symbols, int sample_rate)
// {
//     int samples_per_symbol = sample_rate/static_cast<int>(baud_rate);
//     std::vector<float> pulse = gaussian_pulse(samples_per_symbol, gaussian_bt);    
    
//     int tot_samples = (symbols.size() + 4) * samples_per_symbol; //4 extra bymbol periods of padding
//     std::vector<float> waveform(tot_samples, 0.0f); 

//     std::vector<float> padded_symbols;
//     padded_symbols.push_back(symbols[0]);
//     padded_symbols.insert(padded_symbols.end(), symbols.begin(), symbols.end());
//     padded_symbols.push_back(symbols.back());

//     //eq1 from docs: fd(t) = h*sum(bn*p(t-nT))
//     int s = 0; //start smaple
//     for (int symbol: padded_symbols) {
//         for (size_t i = 0; i<pulse.size() && (s + i) <waveform.size(); ++i){
//             waveform[s + i] += symbol*pulse[i]*freq_shift;
//         }
//         s += samples_per_symbol;
//     }

//     int output_len = symbols.size() * samples_per_symbol;
//     int skip_padding = 2*samples_per_symbol;
    
//     std::vector<float> output(output_len);
//     std::copy(waveform.begin() + skip_padding,
//               waveform.begin() + skip_padding + output_len,
//               output.begin());
   
//     return output;
// }

// std::vector<float>
// ft8_encoder::gaussian_pulse(int samples_per_symbol, float bt)
// {
//     int pulse_len = 3*samples_per_symbol;
//     std::vector<float> pulse(pulse_len);

//     //eq3 from docs: p(t) = (1/2T) * [erf(kBT(t+0.5)/T) - erf(kBT(t-0.5)/T)]
//     //from docs k = 5.336
    
//     float k = std::sqrt(2.0f/std::log(2.0f));

//     float erf_coeff = k*bt;
//     float norm = 0.5f; // 1/2T -> T=1

//     for (int i=0; i<pulse_len; ++i){
//         float t = (static_cast<float>(i) / samples_per_symbol) - 1.5f;
//         float erf_plus = std::erf(erf_coeff * (t+0.5f));
//         float erf_minus = std::erf(erf_coeff * (t-0.5f));
//         pulse[i] = norm * (erf_plus - erf_minus);
//     }
    
//     return pulse;
// }