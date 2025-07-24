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
    field_dayx,  // 0.4
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
  bool is_field_day (const std::vector<std::string> &keywords,
                     bool check_r) const;
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

private:
  mutable gr::logger d_logger;
  std::string d_message;
  message_type d_current_type = message_type::unknown;
  bool d_valid = false;
  bool has_nonstd = false;
  bool has_callsigns = false;
  bool has_extended_grid = false;

  void preprocess_message ();
  void input_validation ();
  void trim ();
  void character_validation ();
};

#endif
