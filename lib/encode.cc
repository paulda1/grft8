/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "encode.h"
#include "message.h"
#include <bitset>
#include <boost/multiprecision/cpp_int.hpp>
#include <cmath>
#include <gmpxx.h>
#include <regex>
#include <string_view>

Encode::Encode() : d_logger("Encoding") {
    d_logger.info("Message encoding created");
}
Encode::Encode(const Message& message) : d_logger("Encoding") {
    d_logger.info("FT8 encoding object constructed");
    bitfields(message);
}

void Encode::bitfields(const Message& message) {
    Message::message_type type = message.message_type_detection();

    switch (type) {
        case Message::message_type::standard: // or euvhf
            encode_standard(message);
            break;
        case Message::message_type::dxpedition:
            encode_dexpedition(message);
            break;
        case Message::message_type::field_day:
            encode_field_day(message);
            break;
        case Message::message_type::telemetry:
            encode_telemetry(message);
            break;
        case Message::message_type::euvhf: // encoded same way as standard
            encode_standard(message);
            break;
        case Message::message_type::unknown:
            d_logger.error("no message");
            break;
        case Message::message_type::rtty_ru:
            encode_rtty_ru(message);
            break;
        case Message::message_type::nonstd_call:
            encode_nonstd_call(message);
            break;
        case Message::message_type::euvhfx:
            encode_euvhfx(message);
            break;
        case Message::message_type::free_text:
            encode_free_text(message);
            break;
    }
}

std::vector<int> Encode::bits_to_fsk8(const std::bitset<174>& ldpc_bits) {
    // from table in docs
    const int gray_map[8] = {0 /*000*/, 1 /*001*/, 3 /*010*/, 2 /*011*/,
                             5 /*101*/, 6 /*110*/, 4 /*100*/, 7 /*111*/};
    std::vector<int> symbols;
    symbols.reserve(58); // (174/3 = 58)

    // process bits in 3
    for (int i = 0; i < 174; i += 3) {
        int bit_trio = 0;
        // converts to 0-7 range
        bit_trio |= (ldpc_bits[i] ? 1 : 0) << 2; // ie. 0000 to 0100 = 4
        bit_trio |= (ldpc_bits[i + 1] ? 1 : 0) << 1;
        bit_trio |= (ldpc_bits[i + 2] ? 1 : 0);

        int channel_symbol = gray_map[bit_trio];
        symbols.push_back(channel_symbol);
    }

    // array from docs
    const std::vector<int> S = {3, 1, 4, 0, 6, 5, 2}; // sync sequence, consta's array
    const std::vector<int> Ma(symbols.begin(), symbols.begin() + 29);
    const std::vector<int> Mb(symbols.begin() + 29, symbols.end());

    // transmission sequence (from docs): S + Ma + S + Mb + S
    std::vector<int> transmit_symbols;
    transmit_symbols.reserve(79); // 3 + 29 + 3 + 39 + 3 = 79

    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());
    transmit_symbols.insert(transmit_symbols.end(), Ma.begin(), Ma.end());
    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());
    transmit_symbols.insert(transmit_symbols.end(), Mb.begin(), Mb.end());
    transmit_symbols.insert(transmit_symbols.end(), S.begin(), S.end());

    return transmit_symbols;
}

std::bitset<174> Encode::apply_ldpc(const std::bitset<91>& crc_bits) {
    auto generator = load_generator_matrix();
    std::bitset<174> complete_msg;

    for (int i = 0; i < 91; i++) {
        complete_msg[i] = crc_bits[i];
    }

    for (int parity_bit = 0; parity_bit < 83; parity_bit++) {
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

std::vector<std::bitset<91>> Encode::load_generator_matrix() {
    // Hex strings representing the 83x91 generator matrixi
    std::vector<std::bitset<91>> generator_matrix;
    generator_matrix.reserve(83);
    
    for (int i = 0; i < 83; i++) {
        std::bitset<91> m_row;
        const char* hex_str = GENERATOR_HEX[i];
        
        // Process 23 hex characters (but only use 91 bits total)
        for (int j = 0; j < 23; j++) {
            char c = hex_str[j];
            int istr;
            
            // Convert hex character to integer
            if (c >= '0' && c <= '9') {
                istr = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                istr = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                istr = c - 'A' + 10;
            } else {
                continue;
            }
            
            // For the last character (j=22), only use 3 bits
            int ibmax = (j == 22) ? 3 : 4;
            
            // Extract bits: test bit positions 3,2,1,0 (MSB to LSB)
            for (int jj = 1; jj <= ibmax; jj++) {
                int icol = j * 4 + jj - 1;  // Convert to 0-indexed
                if (istr & (1 << (4 - jj))) {  // Test bit (4-jj)
                    m_row[icol] = 1;
                }
            }
        }
        
        generator_matrix.push_back(m_row);
    }
    
    d_logger.info("Loaded generator matrix with {} rows", generator_matrix.size());
    
    return generator_matrix;
}

std::bitset<91> Encode::calc_crc(const std::bitset<77>& message_bits) {
    // basically a 14-bit shift register
    // if bits fall of the left edge
    // it trigger the polynomial correction
    // so these other bits in the register are flipped using the XOR
    // it's like a conveyer belt of bits
    
    d_logger.info("The input message for this function:");

    uint16_t polynomial = 0x2757;
    uint16_t crc = 0;
    uint16_t mask_top = (uint16_t)(1u << 13);
    uint16_t mask_crc = ((mask_top << 1) - 1u);
   
    for (int i = 0; i < 82; ++i){
      if (i % 8 == 0){
        uint8_t byte = 0;
        for (int j = 0; j < 8 && (i + j) < 77; ++j) {
            byte |= (message_bits[i + j] << (7-j));
        }
        crc ^= (byte << (14-8));
      }

      if (crc & mask_top){
        crc = (crc << 1) ^ polynomial;
      }

      else {
        crc = (crc << 1);
      }
    }

    crc &= mask_crc;

    std::bitset<91> post_crc_msg;
    for (int i = 0; i < 77; ++i){
      post_crc_msg[i] = message_bits[i];
    } 
    
    std::bitset<14> crc_bits(crc);
    for (int i = 0; i < 14; ++i){
      post_crc_msg[77 + i] = crc_bits[13 - i];
    } 

    return post_crc_msg;
}

void pack_bits(std::bitset<77>& bits, int& bit_pos, uint64_t val, int num_bits) {
    if (bit_pos <= 77) {
        for (int i = 0; i < num_bits; ++i) {
            bits[bit_pos++] = (val >> i) & 1;
        }
    }
}

std::bitset<77> Encode::encode_euvhfx(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint16_t h12 = 0;
    uint32_t h22 = 0;
    bool R1 = 0;
    uint8_t r3 = 0;
    uint16_t s11 = 0;
    uint32_t g25 = 0;
    uint8_t i3 = 5;

    if (parsed_data.callsigns.size() >= 2) {
        auto hash1 = string_to_hash(parsed_data.callsigns[0]);
        h12 = hash1[1]; // Use 12-bit hash

        auto hash2 = string_to_hash(parsed_data.callsigns[1]);
        h22 = hash2[2]; // Use 22-bit hash
    }

    R1 = parsed_data.has_R;
    r3 = encode_contest_report(parsed_data);
    s11 = encode_euvhf_serial(parsed_data);
    g25 = g6_to_25(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;

    pack_bits(message_bits, bit_pos, h12, 12);
    pack_bits(message_bits, bit_pos, h22, 22);
    pack_bits(message_bits, bit_pos, R1, 1);
    pack_bits(message_bits, bit_pos, r3, 3);
    pack_bits(message_bits, bit_pos, s11, 11);
    pack_bits(message_bits, bit_pos, g25, 25);
    pack_bits(message_bits, bit_pos, i3, 3);

    d_logger.info("77-bit EU VHFx message assembled");
    return message_bits;
}

std::bitset<77> Encode::encode_free_text(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    std::bitset<71> f71;
    uint8_t n3 = 0;
    uint8_t i3 = 0;

    f71 = free_text_to_f71(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;

    for (int i = 0; i <= 71; ++i) {                                   
      message_bits[bit_pos++] = f71[i];
    }

    pack_bits(message_bits, bit_pos, n3, 3);
    pack_bits(message_bits, bit_pos, i3, 3);

    d_logger.info("77-bit free text message assembled: '{}'", parsed_data.free_text);
    return message_bits;
}

std::bitset<77> Encode::encode_nonstd_call(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint16_t h12 = 0;
    uint64_t c58 = 0;
    bool h1 = 0;
    uint8_t r2 = 0;
    uint8_t c1 = 0;
    uint8_t i3 = 4;

    c1 = parsed_data.has_cq ? 1 : 0;
    c58 = nonstd_to_58(parsed_data);

    std::string std_callsign;
    for (const auto& callsign : parsed_data.callsigns) {
        if (callsign.find('/') == std::string::npos) {
            std_callsign = callsign;
            break; // assign the parsed standard call sign if any, immediately
        }
    }

    if (!std_callsign.empty()) {
        auto hash_values = string_to_hash(std_callsign);
        h12 = hash_values[1];           // Use 12-bit hash
        h1 = (hash_values[0] >> 9) & 1; // Use 1 bit from 10-bit hash (I'll double check this later)
    }

    r2 = encode_r2(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;

    pack_bits(message_bits, bit_pos, h12, 12);
    pack_bits(message_bits, bit_pos, c58, 58);
    pack_bits(message_bits, bit_pos, h1, 1);
    pack_bits(message_bits, bit_pos, r2, 2);
    pack_bits(message_bits, bit_pos, c1, 1);
    pack_bits(message_bits, bit_pos, i3, 3);

    d_logger.info("77-bit nonstandard call message assembled");
    return message_bits;
}

std::bitset<77> Encode::encode_telemetry(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint8_t i3 = 0;

    if (parsed_data.telemetry_hex.empty()) {
        d_logger.error("No telemetry data found");
        return std::bitset<77>();
    }

    std::string hex_data = parsed_data.telemetry_hex;

    if (hex_data.length() > 18) {
        d_logger.error("Telemetry hex data too long");
        return std::bitset<77>();
    }

    if (hex_data.length() == 18) {
        char first_digit = hex_data[0];
        if (first_digit < '0' || first_digit > '7') {
            d_logger.error("First digit of 18 char telemetry must be 0-7");
            return std::bitset<77>();
        }
    }

    std::bitset<77> message_bits;

    int bit_pos = 0;
    for (int i = hex_data.length() - 1; i >= 0; --i) {
        char hex_char = hex_data[i];
        int hex_value;

        if (hex_char >= '0' && hex_char <= '9') {
            hex_value = hex_char - '0';
        } else if (hex_char >= 'A' && hex_char <= 'F') {
            hex_value = hex_char - 'A' + 10;
        } else if (hex_char >= 'a' && hex_char <= 'f') {
            hex_value = hex_char - 'a' + 10;
        } else {
            d_logger.error("Invalid hex character");
            return std::bitset<77>();
        }

        for (int bit = 3; bit >= 0; --bit) {
            if (bit_pos < 71) { // can't use pack bit bc of the 64 bit integer
                message_bits[bit_pos] = (hex_value >> bit) & 1;
                bit_pos++;
            }
        }
    }

    bit_pos = 71;
    pack_bits(message_bits, bit_pos, i3, 3);

    return message_bits;
}

std::bitset<77> Encode::encode_rtty_ru(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    bool t1 = 0;
    uint32_t c28a = 0, c28b = 0;
    bool R1 = 0;
    uint8_t r3 = 0;
    uint16_t s13 = 0;
    uint8_t i3 = 3;

    t1 = parsed_data.has_tu;

    size_t callsign_idx = 0;
    c28a = encode_28(parsed_data, callsign_idx);
    c28b = encode_28(parsed_data, callsign_idx);

    R1 = parsed_data.has_R;
    r3 = encode_contest_report(parsed_data);
    s13 = encode_contest_info(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;

    pack_bits(message_bits, bit_pos, t1, 1);
    pack_bits(message_bits, bit_pos, c28a, 28);
    pack_bits(message_bits, bit_pos, c28b, 28);
    pack_bits(message_bits, bit_pos, R1, 1);
    pack_bits(message_bits, bit_pos, r3, 3);
    pack_bits(message_bits, bit_pos, s13, 13);
    pack_bits(message_bits, bit_pos, i3, 3);

    return message_bits;
}

std::bitset<77> Encode::encode_field_day(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint32_t c28a = 0, c28b = 0;
    bool R1 = 0;
    // in docs n4 specifies the number of transmitters but for now we'll have only 1
    uint8_t n4 = 1;
    uint8_t k3 = 0;
    uint8_t S7 = 0;
    uint8_t i3 = 0;

    size_t callsign_idx = 0;
    c28a = encode_28(parsed_data, callsign_idx);
    c28b = encode_28(parsed_data, callsign_idx);

    R1 = parsed_data.has_R;
    k3 = encode_fdclass(parsed_data);
    S7 = encode_arrl_section(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;
    pack_bits(message_bits, bit_pos, c28a, 28);
    pack_bits(message_bits, bit_pos, c28b, 28);
    pack_bits(message_bits, bit_pos, R1, 1);
    pack_bits(message_bits, bit_pos, n4, 4);
    pack_bits(message_bits, bit_pos, k3, 3);
    pack_bits(message_bits, bit_pos, S7, 7);
    pack_bits(message_bits, bit_pos, i3, 3);

    return message_bits;
}

std::bitset<77> Encode::encode_dexpedition(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint32_t c28a = 0, c28b = 0;
    uint16_t h10 = 0;
    uint8_t r5 = 0;
    uint8_t i3 = 0;

    size_t callsign_idx = 0;

    c28a = encode_28(parsed_data, callsign_idx);
    c28b = encode_28(parsed_data, callsign_idx);

    std::string text_to_hash;
    if (parsed_data.has_rr73) {
        text_to_hash = "RR73";
    } else if (parsed_data.has_73) {
        text_to_hash = "73";
    } else if (parsed_data.has_rrr) {
        text_to_hash = "RRR";
    }

    if (!text_to_hash.empty()) {
        auto hash_values = string_to_hash(text_to_hash);
        h10 = hash_values[0];
    }

    r5 = encode_sigreport(parsed_data);

    std::bitset<77> message_bits;
    int bit_pos = 0;
    pack_bits(message_bits, bit_pos, c28a, 28);
    pack_bits(message_bits, bit_pos, c28b, 28);
    pack_bits(message_bits, bit_pos, h10, 10);
    pack_bits(message_bits, bit_pos, r5, 5);
    pack_bits(message_bits, bit_pos, i3, 3);

    return message_bits;
}

std::bitset<77> Encode::encode_standard(const Message& message) {
    const auto& parsed_data = message.get_parsed_data();
    uint32_t c28a = 0, c28b = 0;
    bool r1 = 0, R1 = 0;
    uint16_t g15 = 0;
    uint8_t i3 = 1;

    size_t callsign_index = 0;
    // encode first call sign
    c28a = encode_28(parsed_data, callsign_index);
    // encode second call sign
    c28b = encode_28(parsed_data, callsign_index);
    g15 = g4_to_15(parsed_data);
    r1 = parsed_data.has_r;
    R1 = parsed_data.has_R;

    std::bitset<77> message_bits;
    int bit_pos = 0;

    pack_bits(message_bits, bit_pos, c28a, 28);
    pack_bits(message_bits, bit_pos, c28b, 28);
    pack_bits(message_bits, bit_pos, r1, 1);
    pack_bits(message_bits, bit_pos, R1, 1);
    pack_bits(message_bits, bit_pos, g15, 15);
    pack_bits(message_bits, bit_pos, i3, 3);

    return message_bits;
}

uint16_t Encode::encode_euvhf_serial(const Message::ParsedData& parsed_data) {
    if (parsed_data.contest_info.empty()) {
        return 0;
    }

    if (std::regex_match(parsed_data.contest_info, std::regex(R"(\d{1,4})"))) {
        int serial = std::stoi(parsed_data.contest_info);
        if (serial >= 0 && serial <= 2047) {
            return serial;
        }
    }

    return 0;
}

uint32_t Encode::g6_to_25(const Message::ParsedData& parsed_data) {
    // Look for 6-character grid squares
    for (const auto& grid : parsed_data.grid_squares) {
        if (grid.length() == 6) {
            // Convert 6-char grid to 25-bit value
            uint32_t encoded = (grid[0] - 'A') * 18 * 10 * 10 * 24 * 24 +
                               (grid[1] - 'A') * 10 * 10 * 24 * 24 +
                               (grid[2] - '0') * 10 * 24 * 24 + (grid[3] - '0') * 24 * 24 +
                               (grid[4] - 'A') * 24 + (grid[5] - 'A');
            return encoded;
        }
    }
    return 0;
}

uint8_t Encode::encode_contest_report(const Message::ParsedData& parsed_data) {
    if (parsed_data.contest_report.empty()) {
        return 0;
    }

    std::string report = parsed_data.contest_report;

    // 3-digit reports (529-599)
    if (report.length() == 3) {
        int value = std::stoi(report);
        if (value >= 529 && value <= 599 && value % 10 == 9) {
            return (value - 529) / 10; // 529->0, 539->1, ..., 599->7
        }
    }
    // 2-digit reports (52-59)
    else if (report.length() == 2) {
        int value = std::stoi(report);
        if (value >= 52 && value <= 59) {
            return value - 52; // 52->0, 53->1, ..., 59->7
        }
    }

    return 0;
}

uint16_t Encode::encode_contest_info(const Message::ParsedData& parsed_data) {
    if (parsed_data.contest_info.empty()) {
        return 0;
    }

    // Check if it's a state/province
    int state_idx = parsed_data.states_provinces;
    if (state_idx != -1) {
        return 8001 + state_idx; // Values 8001-8065 for states/provinces
    }
    std::string info = parsed_data.contest_info;
    // Check if it's a serial number (0-7999)
    if (std::regex_match(info, std::regex(R"(\d{1,4})"))) {
        int serial = std::stoi(info);
        if (serial >= 0 && serial <= 7999) {
            return serial;
        }
    }

    return 0;
}

uint32_t Encode::encode_28(const Message::ParsedData& parsed_data, size_t& callsign_idx) {
    if (parsed_data.has_de) {
        return 0;
    }
    if (parsed_data.has_qrz) {
        return 1;
    }

    if (parsed_data.has_cq) {
        if (parsed_data.cq_modifiers.empty()) {
            return 2; // Plain CQ
        }

        const std::string& modifier = parsed_data.cq_modifiers[0];

        // Check if it's a number
        if (std::regex_match(modifier, std::regex(R"(\d{1,3})"))) {
            uint32_t number = std::stoul(modifier);
            if (number <= 999) {
                return 3 + number;
            }
        }

        // Check if it's letters
        if (std::regex_match(modifier, std::regex(R"([A-Z]{1,4})"))) {
            uint32_t letter_val = 0;
            switch (modifier.length()) {
                case 1:
                    letter_val = modifier[0] - 'A';
                    return 1004 + letter_val;
                case 2:
                    letter_val = (modifier[0] - 'A') * 26 + (modifier[1] - 'A');
                    return 1031 + letter_val;
                case 3:
                    letter_val = (modifier[0] - 'A') * 26 * 26 + (modifier[1] - 'A') * 26 +
                                 (modifier[2] - 'A');
                    return 1760 + letter_val;
                case 4:
                    letter_val = (modifier[0] - 'A') * 26 * 26 * 26 +
                                 (modifier[1] - 'A') * 26 * 26 + (modifier[2] - 'A') * 26 +
                                 (modifier[3] - 'A');
                    return 21443 + letter_val;
            }
        }

        return 2;
    }

    // Use callsigns in order
    if (callsign_idx < parsed_data.callsigns.size()) {
        std::string callsign = parsed_data.callsigns[callsign_idx];
        callsign_idx++;
        return std_call_to_28(callsign);
    }

    return 0;
}

uint32_t Encode::std_call_to_28(const std::string& callsign) {
    constexpr std::string_view a1 =
        " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // space, numbers, letters
    constexpr std::string_view a2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // numbers, letters
    constexpr std::string_view a3 = "0123456789";                           // numbers
    constexpr std::string_view a4 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";          // space, letters

    std::string temp_msg = callsign;
    temp_msg.resize(6, ' '); // max size 6 chars

    size_t i1 = a1.find(temp_msg[0]);
    size_t i2 = a2.find(temp_msg[1]);
    size_t i3 = a3.find(temp_msg[2]);
    size_t i4 = a4.find(temp_msg[3]);
    size_t i5 = a4.find(temp_msg[4]);
    size_t i6 = a4.find(temp_msg[5]);

    constexpr size_t ntokens = 2063592;
    constexpr size_t max22 = 4194304;

    if (i1 == std::string_view::npos || i2 == std::string_view::npos ||
        i3 == std::string_view::npos || i4 == std::string_view::npos ||
        i5 == std::string_view::npos || i6 == std::string_view::npos) {
        d_logger.error("Invalid character trying to be encoded in standard message");
    }

    uint32_t n28 = ntokens + max22 + 36 * 10 * 27 * 27 * 27 * i1 + 10 * 27 * 27 * 27 * i2 +
                   27 * 27 * 27 * i3 + 27 * 27 * i4 + 27 * i5 + i6;

    return n28;
}

std::array<uint32_t, 3> Encode::string_to_hash(const std::string& string) {
    constexpr std::string_view c = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // base 38
    constexpr uint64_t prime = 4705583345958ULL;
    constexpr std::array<int, 3> nbits = {10, 12, 22};
    constexpr uint32_t ntokens = 2063592;

    std::string temp_string = string;
    temp_string.resize(11, ' ');

    std::array<uint64_t, 3> n38 = {0, 0, 0};
    std::array<uint32_t, 3> hash_vals;

    for (int k = 0; k < 3; ++k) {
        n38[k] = 0;
        for (int i = 0; i < 11; ++i) {
            size_t j = c.find(temp_string[i]);
            if (j == std::string_view::npos) {
                d_logger.error("Invalid character");
                return {0, 0, 0};
            }
            n38[k] = 38 * n38[k] + j; // convert to base 38
        }

        uint64_t product = prime * n38[k];
        int right_shift_amt = 64 - nbits[k];
        hash_vals[k] = static_cast<uint32_t>(product >> right_shift_amt);
    }

    uint32_t h22_biased = hash_vals[2] + ntokens;
    return {hash_vals[0], hash_vals[1], h22_biased};
}

uint8_t Encode::encode_arrl_section(const Message::ParsedData& parsed_data) {
    if (parsed_data.arrl_section >= 0) {
        return static_cast<uint8_t>(parsed_data.arrl_section);
    }
    return 0;
}

uint64_t Encode::nonstd_to_58(const Message::ParsedData& parsed_data) {
    constexpr std::string_view a = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";
    uint64_t n58 = 0;

    // first non-standard callsign
    std::string callsign_to_encode;
    for (const auto& callsign : parsed_data.callsigns) {
        if (callsign.find('/') != std::string::npos) {
            callsign_to_encode = callsign;
            break;
        }
    }

    if (callsign_to_encode.empty()) {
        d_logger.error("No non-standard callsign found");
        return 0;
    }

    std::string temp_msg = callsign_to_encode;
    temp_msg.resize(11, ' ');

    // Convert to base-38 number
    for (char c : temp_msg) {
        size_t i = a.find(c);
        if (i == std::string_view::npos) {
            d_logger.error("Invalid character for non-standard message: {}", c);
            return 0;
        }
        n58 = n58 * a.length() + i;
    }

    return n58;
}

std::bitset<71> Encode::free_text_to_f71(const Message::ParsedData& parsed_data) {
    constexpr std::string_view a = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?";

    std::string temp_msg = parsed_data.free_text;
    temp_msg.resize(13, ' ');

    mpz_class value = 0;

    for (char c : temp_msg) {
        size_t i = a.find(c);
        if (i == std::string_view::npos) {
            d_logger.error("Invalid character for free text message");
            i = 0;
        }
        value = value * 42 + i;
    }

    std::bitset<71> bits;

    for (size_t i = 0; i < 71; ++i) {
        bits[70 - i] = mpz_tstbit(value.get_mpz_t(), i); //MSB first
    }

    return bits;
}
// in original protocol RRR, RR73, and 73 are also encoded here, removed
// because it is redundant

uint16_t Encode::g4_to_15(const Message::ParsedData& parsed_data) {
    if (!parsed_data.grid_squares.empty()) {
        const std::string& grid = parsed_data.grid_squares[0];
        return (grid[0] - 'A') * 18 * 10 * 10 + (grid[1] - 'A') * 10 * 10 + (grid[2] - '0') * 10 +
               (grid[3] - '0');
    }
    return 0;
}

uint8_t Encode::encode_fdclass(const Message::ParsedData& parsed_data) {
    if (!parsed_data.field_day_class.empty()) {
        char fd_class = parsed_data.field_day_class.back(); // Last character
        if (fd_class >= 'A' && fd_class <= 'F') {
            return fd_class - 'A';
        }
    }
    return 0;
}

uint8_t Encode::encode_r2(const Message::ParsedData& parsed_data) {
    if (parsed_data.has_rrr)
        return 1;
    if (parsed_data.has_rr73)
        return 2;
    if (parsed_data.has_73)
        return 3;
    return 0;
}

uint8_t Encode::encode_sigreport(const Message::ParsedData& parsed_data) {
    if (parsed_data.signal_report.empty()) {
        return 0;
    }

    int db = std::stoi(parsed_data.signal_report);
    return (db + 30) / 2;
}
