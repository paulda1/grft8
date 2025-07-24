/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FT8_ENCODER_H
#define FT8_ENCODER_H

#include <ft8/api.h>
#include <bitset>
#include <gnuradio/logger.h>
#include <string>
#include <vector>
#include <gmp.h>
#include "message.h"

namespace gr
{
class logger;
}

class FT8_API ft8_encoder
{
public:
  ft8_encoder ();
  ft8_encoder (const message &message);
 
  // std::vector<float> generate_ft8_waveform (const std::vector<int>& symbols, int sample_rate);
  // std::vector<float> gaussian_pulse (int samples_per_symbol, float bt);
 
  uint32_t encode_28 (const message::ParsedData& parsed_data, size_t& callsign_idx);
  uint32_t std_call_to_28 (const std::string &callsign);
  uint16_t g4_to_15 (const message::ParsedData& parsed_data);
  std::array<uint32_t, 3> string_to_hash(const std::string& string);

  std::bitset<77> encode_standard (const message &message);
  std::bitset<77> encode_dexpedition (const message &message);
  std::bitset<77> encode_field_day (const message &message);
  std::bitset<77> encode_telemetry(const message &message);
  std::bitset<77> encode_rtty_ru(const message &message);
  std::bitset<77> encode_nonstd_call (const message &message);
  std::bitset<77> encode_euvhfx (const message &message);
  std::bitset<77> encode_free_text (const message &message);

  std::bitset<91> calc_crc (const std::bitset<77>& message_bits);
  std::bitset<174> apply_ldpc (const std::bitset<91>& crc_bits);
  std::vector<std::bitset<91>> load_generator_matrix (const std::string& filename);
  std::vector<int> bits_to_fsk8 (const std::bitset<174>& ldpc_bits);


  uint64_t nonstd_to_58 (const message::ParsedData& parsed_data);
  std::bitset<71> free_text_to_f71 (const message::ParsedData& parsed_data);
  uint32_t g6_to_25(const message::ParsedData& parsed_data);

  uint8_t encode_fdclass (const message::ParsedData& parsed_data);
  uint8_t encode_r2 (const message::ParsedData& parsed_data);
  uint8_t encode_sigreport (const message::ParsedData& parsed_data);
  uint8_t encode_arrl_section(const message::ParsedData& parsed_data);
  uint8_t encode_contest_report(const message::ParsedData& parsed_data);
  uint16_t encode_contest_info(const message::ParsedData& parsed_data);
  uint16_t encode_euvhf_serial(const message::ParsedData& parsed_data);
  int get_state_province_idx(const std::string &keyword);

  static constexpr int sample_rate_const = 48000; 
  static constexpr int samples_per_symbol = 7680; //48000/6.25
  static constexpr int tot_symbols = 79;
  static constexpr float gaussian_bt = 2.0f;
  static constexpr float baud_rate = 6.25f;
  static constexpr float freq_shift = 6.25f;

private:
  gr::logger d_logger;
  void bitfields (const message &message);
};

#endif
