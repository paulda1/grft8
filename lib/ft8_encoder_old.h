/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FT8_ENCODER_H
#define FT8_ENCODER_H

#include <ft8/api.h>
#include "message.h"
#include <bitset>
#include <gnuradio/logger.h>
#include <string>
#include <vector>
#include <gmp.h>

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
 
  uint32_t encode_28 (std::string &temp_msg, const message &message);
  uint32_t std_call_to_28 (std::string &msg);
  uint16_t g4_to_15 (std::string &temp_msg, const message &message);

  std::bitset<77> encode_standard (const message &message);
  //void encode_dexpedition (const message &message);
  //void encode_field_day (const message &message);
  //void encode_field_dayx (const message &message);
  //void encode_telemetry (const message &message);
  //void encode_rtty_ru (const message &message);
  //void encode_nonstd_call (const message &message);
  //void encode_euvhfx (const message &message);
  //void encode_free_text (const message &message);

  // std::vector<float> encode_ft8_complete (std::bitset<77> message_bits);
  std::bitset<91> calc_crc (const std::bitset<77>& message_bits);
  std::bitset<174> apply_ldpc (const std::bitset<91>& crc_bits);
  std::vector<std::bitset<91>> load_generator_matrix (const std::string& filename);
  std::vector<int> bits_to_fsk8 (const std::bitset<174>& ldpc_bits);

  uint64_t nonstd_to_58 (std::string &msg);
  std::bitset<71> free_text_to_f71 (std::string &msg);

  uint32_t g6_to_15 (const message &message);

  bool encode_fdclass (char fd_class);
  bool encode_p1 (std::string &msg);
  bool encode_r1 (std::string &msg);
  bool encode_R1 (std::string &msg);
  bool encode_t1 (std::string &msg);
  uint8_t encode_r2 (std::string &msg);
  uint8_t encode_sigreport (std::string &msg);
    
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
