/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <ft8/api.h>
#include <gnuradio/logger.h>
#include <string>
#include <vector>

namespace gr
{
class logger;
}

class FT8_API message
{
public:
  enum class message_type
  {
    free_text,   // 0.0
    dxpedition,  // 0.1
    field_day,   // 0.3
    telemetry,   // 0.5
    standard,    // 1.0
    euvhf,       // 2.0
    rtty_ru,     // 3.0
    nonstd_call, // 4.0
    euvhfx,      // 5.0
    unknown
  };

  message ();
  explicit message (const std::string &message);

  void parse_message (const std::string &message);
  message_type message_type_detection () const;
  const std::string &
  get_message () const
  {
    return d_message;
  }

  bool is_signal_report (const std::string &token) const;
  bool is_dxpedition (const std::vector<std::string> &keywords) const;
  bool is_callsign (const std::string &keyword) const;
  bool is_hex (const std::string &keyword) const;
  bool is_field_day (const std::vector<std::string> &keywords, bool check_r) const;
  bool is_field_day_class (const std::string &keyword) const;
  bool is_nonstd_callsign (const std::string &keyword) const;
  bool is_telemetry (const std::vector<std::string> &keywords) const;
  bool is_std (const std::vector<std::string> &keywords) const;
  bool is_rtty_ru (const std::vector<std::string> &keywords) const;
  bool is_contest (const std::string &keyword) const;
  bool is_euvhfx (const std::vector<std::string> &keywords) const;
  bool is_grid_square (const std::string &keyword) const;
  bool is_grid_6square (const std::string &keyword) const;
  bool is_nonstd (const std::vector<std::string> &keywords) const;
  bool is_arrl_section(const std::string &keyword) const;
  bool is_contest_report(const std::string &keyword) const;
  bool is_state_province(const std::string &keyword) const;
  int get_state_province_idx(const std::string &keyword) const;
  int get_arrl_section_idx(const std::string &keyword) const;

  static const std::vector<std::string> sections;
  static const std::vector<std::string> states_provinces;

  struct ParsedData {
    std::vector<std::string> callsigns;
    std::vector<std::string> grid_squares;
    std::vector<std::string> cq_modifiers;
    std::string signal_report;
    std::string field_day_class;
    bool has_de = false;
    bool has_qrz = false;
    bool has_cq = false;
    bool has_r = false;
    bool has_R = false;
    bool has_p = false;
    bool has_rrr = false;
    bool has_rr73 = false;
    bool has_73 = false;
    bool has_tu = false;
    std::string contest_report;
    std::string contest_info;
    std::string telemetry_hex;
    std::string free_text;
    int states_provinces = -1;
    int arrl_section = -1;
  };

  const ParsedData& get_parsed_data() const { return d_parsed_data; }

private:
  mutable gr::logger d_logger;
  std::string d_message;
  message_type d_current_type = message_type::unknown;
  bool d_valid = false;
  bool has_nonstd = false;
  bool has_callsigns = false;
  bool has_extended_grid = false;
  mutable ParsedData d_parsed_data;

  void preprocess_message ();
  void input_validation ();
  void trim ();
  void character_validation ();
};

#endif
