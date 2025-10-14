/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "message.h"
#include <cctype>
#include <sstream>
#include <regex>

// from original doc's .txt list
const std::vector<std::string> Message::sections = {
    "AB",  "AK",  "AL",  "AR",  "AZ",  "BC",  "CO",  "CT",  "DE",  "EB",  "EMA", "ENY",
    "EPA", "EWA", "GA",  "GTA", "IA",  "ID",  "IL",  "IN",  "KS",  "KY",  "LA",  "LAX",
    "MAR", "MB",  "MDC", "ME",  "MI",  "MN",  "MO",  "MS",  "MT",  "NC",  "ND",  "NE",
    "NFL", "NH",  "NL",  "NLI", "NM",  "NNJ", "NNY", "NT",  "NTX", "NV",  "OH",  "OK",
    "ONE", "ONN", "ONS", "OR",  "ORG", "PAC", "PR",  "QC",  "RI",  "SB",  "SC",  "SCV",
    "SD",  "SDG", "SF",  "SFL", "SJV", "SK",  "SNJ", "STX", "SV",  "TN",  "UT",  "VA",
    "VI",  "VT",  "WCF", "WI",  "WMA", "WNY", "WPA", "WTX", "WV",  "WWA", "WY",  "DX"};

const std::vector<std::string> Message::states_provinces = {
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT",  "DE", "FL", "GA", "HI", "ID",  "IL",
    "IN", "IA", "KS", "KY", "LA", "ME", "MD",  "MA", "MI", "MN", "MS", "MO",  "MT",
    "NE", "NV", "NH", "NJ", "NM", "NY", "NC",  "ND", "OH", "OK", "OR", "PA",  "RI",
    "SC", "SD", "TN", "TX", "UT", "VT", "VA",  "WA", "WV", "WI", "WY", "NB",  "NS",
    "QC", "ON", "MB", "SK", "AB", "BC", "NWT", "NF", "LB", "NU", "YT", "PEI", "DC"};

Message::Message() : d_logger("Message_Parsing") {
    d_logger.info("Message object constructed");
}
Message::Message(const std::string& message) : d_logger("Message_Parsing") {
    d_logger.info("Message object constructed, message '{}'", message);
    parse_message(message);
}

void Message::parse_message(const std::string& message) {
    d_message = message;
    preprocess_message();
}
void Message::preprocess_message() {
    input_validation();
    message_type_detection();
    d_logger.info("Preprocessed message: {}", d_message);
}

void Message::input_validation() {
    if (d_message.empty()) {
        d_logger.error("No message input");
        return;
    }

    trim();
    character_validation();
}

void Message::trim()
// remove leading and trailing whitespace
{
    size_t start = 0;
    while (start < d_message.length() && std::isspace(d_message[start])) {
        start++;
    }

    size_t end = d_message.length();
    while (end > start && std::isspace(d_message[end - 1])) {
        end--;
    }

    d_message = d_message.substr(start, end - start);
}

void Message::character_validation()
// only A-Z, 0-9, and / is allowed, no more than one consecutive space
{
    size_t i = 0;
    bool last_was_space = false;
    for (size_t j = 0; j < d_message.length(); ++j) {
        char c = d_message[j];

        if (std::isalpha(c)) {
            c = std::toupper(c);
        }
        if (!(std::isalpha(c) || std::isdigit(c) || c == ' ' || c == '+' || c == '-' || c == '/' ||
              c == '.' || c == '?')) {
            d_logger.error("Invalid character: {}", c);
            return;
        }
        if (!(last_was_space && c == ' ')) {
            d_message[i] = c;
            i++;
        }
        last_was_space = (c == ' ');
    }

    d_message.resize(i);
}

Message::message_type Message::message_type_detection() const {
    std::istringstream stream(d_message);
    std::string input;
    std::vector<std::string> keywords;
    size_t total_chars = 0;

    while (stream >> input) {
        keywords.push_back(input);
        total_chars += input.length();
    }

    d_parsed_data = ParsedData{};

    // Parse all tokens and populate d_parsed_data
    for (const auto& keyword : keywords) {
        if (keyword == "DE") {
            d_parsed_data.has_de = true;
        } else if (keyword == "QRZ") {
            d_parsed_data.has_qrz = true;
        } else if (keyword == "CQ") {
            d_parsed_data.has_cq = true;
        } else if (keyword == "R") {
            d_parsed_data.has_R = true;
        } else if (keyword == "RRR") {
            d_parsed_data.has_rrr = true;
        } else if (keyword == "RR73") {
            d_parsed_data.has_rr73 = true;
        } else if (keyword == "73") {
            d_parsed_data.has_73 = true;
        } else if (keyword == "TU") {
            d_parsed_data.has_tu = true;
        } else if (is_callsign(keyword)) {
            d_parsed_data.callsigns.push_back(keyword);
        } else if (is_grid_square(keyword)) {
            d_parsed_data.grid_squares.push_back(keyword);
        } else if (is_signal_report(keyword)) {
            d_parsed_data.signal_report = keyword;
        } else if (is_contest_report(keyword)) {
            d_parsed_data.contest_report = keyword;
        } else if (is_field_day_class(keyword)) {
            d_parsed_data.field_day_class = keyword;
        } else if (is_arrl_section(keyword)) {
            d_parsed_data.arrl_section = get_arrl_section_idx(keyword);
        } else if (is_state_province(keyword)) {
            d_parsed_data.contest_info = keyword;
            d_parsed_data.states_provinces = get_state_province_idx(keyword);
        } else if (std::regex_match(keyword, std::regex(R"(\d{1,3})"))) {
            d_parsed_data.cq_modifiers.push_back(keyword);
        } else if (std::regex_match(keyword, std::regex(R"([A-Z]{1,4})"))) {
            d_parsed_data.cq_modifiers.push_back(keyword);
        } else if (std::regex_match(keyword, std::regex(R"(\d{1,4})"))) {
            d_parsed_data.contest_info = keyword;
        } else if (keyword.find("/R") != std::string::npos) {
            d_parsed_data.has_r = true;
        } else if (keyword.find("/P") != std::string::npos) {
            d_parsed_data.has_p = true;
        } else if (is_hex(keyword) && keywords.size() == 1) {
            d_parsed_data.telemetry_hex = keyword;
        }
    }

    // If no structured data found and short enough, treat as free text
    if (d_parsed_data.callsigns.empty() && d_parsed_data.grid_squares.empty() &&
        !d_parsed_data.has_cq && total_chars <= 13) {
        d_parsed_data.free_text = d_message;
    }

    // Now determine message type based on parsed data
    message_type current_type = message_type::unknown;

    if (is_dxpedition(keywords)) {
        current_type = message_type::dxpedition;
    } else if (!d_parsed_data.telemetry_hex.empty()) {
        current_type = message_type::telemetry;
    } else if (is_field_day(keywords, false)) {
        current_type = message_type::field_day;
    } else if (is_std(keywords)) {
        current_type = message_type::standard;
    } else if (is_rtty_ru(keywords)) {
        current_type = message_type::rtty_ru;
    } else if (is_euvhfx(keywords)) {
        current_type = message_type::euvhfx;
    } else if (is_nonstd(keywords)) {
        current_type = message_type::nonstd_call;
    } else if (!d_parsed_data.free_text.empty()) {
        current_type = message_type::free_text;
    }

    return current_type;
}

bool Message::is_arrl_section(const std::string& keyword) const {
    return std::find(Message::sections.begin(), Message::sections.end(), keyword) !=
           Message::sections.end();
}

int Message::get_arrl_section_idx(const std::string& keyword) const {
    auto val = std::find(Message::sections.begin(), Message::sections.end(), keyword);
    if (val != sections.end()) {
        return std::distance(Message::sections.begin(), val);
    }
    return -1; // Not found
}

bool Message::is_nonstd(const std::vector<std::string>& keywords) const {
    bool has_nonstd = false;
    for (const auto& keyword : keywords) {
        if (is_nonstd_callsign(keyword)) {
            has_nonstd = true;
        }
    }
    return has_nonstd;
}

bool Message::is_euvhfx(const std::vector<std::string>& keywords) const {
    bool has_callsigns = false;
    bool has_extended_grid = false;

    for (const auto& keyword : keywords) {
        if (is_callsign(keyword)) {
            has_callsigns = true;
        } else if (is_grid_6square(keyword)) {
            has_extended_grid = true;
        }
    }
    return has_callsigns && has_extended_grid;
}

bool Message::is_contest_report(const std::string& keyword) const {
    if (keyword.length() == 2) {
        std::regex two_digit(R"(^5[2-9]$)");
        return std::regex_match(keyword, two_digit);
    } else if (keyword.length() == 3) {
        std::regex three_digit(R"(^5[2-9]9$)");
        return std::regex_match(keyword, three_digit);
    }
    return false;
}

bool Message::is_state_province(const std::string& keyword) const {
    return std::find(states_provinces.begin(), states_provinces.end(), keyword) !=
           states_provinces.end();
}

int Message::get_state_province_idx(const std::string& keyword) const {
    auto it = std::find(states_provinces.begin(), states_provinces.end(), keyword);
    if (it != states_provinces.end()) {
        return std::distance(states_provinces.begin(), it);
    }
    return -1;
}

bool Message::is_rtty_ru(const std::vector<std::string>& keywords) const {
    bool has_callsigns = false;
    bool has_contest_data = false;

    for (const auto& keyword : keywords) {
        if (is_callsign(keyword)) {
            has_callsigns = true;
        } else if (is_contest_report(keyword) || is_state_province(keyword) ||
                   std::regex_match(keyword, std::regex(R"(\d{1,4})"))) {
            has_contest_data = true;
        }
    }
    return has_callsigns && has_contest_data;
}

bool Message::is_contest(const std::string& keyword) const {
    std::regex contest(R"(^[0-9]{3}$)");
    return std::regex_match(keyword, contest);
}

bool Message::is_std(const std::vector<std::string>& keywords) const {
    bool has_callsigns = false;
    bool has_grid = false;

    for (const auto& keyword : keywords) {
        if (is_callsign(keyword)) {
            has_callsigns = true;
        }
        if (is_grid_square(keyword)) {
            has_grid = true;
        }
    }
    return has_callsigns && has_grid;
}

bool Message::is_field_day(const std::vector<std::string>& keywords, bool check_r) const {
    bool has_callsigns = false;
    bool has_field_day_class = false;
    bool has_r = false;

    for (const auto& keyword : keywords) {
        if (is_field_day_class(keyword)) {
            has_field_day_class = true;
        } else if (keyword == "R") {
            has_r = true;
        } else if (is_callsign(keyword)) {
            has_callsigns = true;
        }
    }

    if (!has_callsigns || !has_field_day_class) {
        return false;
    }
    if (check_r && !has_r) {
        return false;
    }
    return true;
}

bool Message::is_field_day_class(const std::string& keyword) const {
    std::regex fdclass(R"(^\d+[ABCDEF]$)");
    return std::regex_match(keyword, fdclass);
}

bool Message::is_telemetry(const std::vector<std::string>& keywords) const {
    if (keywords.size() == 1 && is_hex(keywords[0])) {
        return true;
    }
    return false;
}

bool Message::is_hex(const std::string& keyword) const {
    for (char c : keyword) {
        if (!std::isxdigit(c))
            return false;
    }
    return true;
}

bool Message::is_dxpedition(const std::vector<std::string>& keywords) const {
    for (const auto& keyword : keywords) {
        if (keyword == "RRR" || keyword == "RR73" || keyword == "73" || is_signal_report(keyword)) {
            return true;
        } else if (is_callsign(keyword)) {
            d_logger.info("Callsign detected: {}", keyword);
        }
    }
    return false;
}

bool Message::is_signal_report(const std::string& keyword) const {
    //+-nn
    if (keyword.size() == 3) {
        static const std::regex sreport(R"(^[+-]\d{2}$)");
        return std::regex_match(keyword, sreport);
    }
    return false;
}

bool Message::is_callsign(const std::string& keyword) const {
    // one-two character prefix, at least one is a letter
    // then a decimal digit, and a suffix up to three letters

    static const std::regex callsign(
        R"(^[A-Z][A-Z0-9]?[0-9][A-Z]{1,3}$|^[A-Z0-9][A-Z][0-9][A-Z]{1,3}$)");
    return std::regex_match(keyword, callsign);
}

bool Message::is_nonstd_callsign(const std::string& keyword) const {
    static const std::regex prefix(R"(^[A-Z0-9]{2,4}/[A-Z0-9]{1,2}[A-Z]{1,3}$)");
    static const std::regex suffix(R"(^[A-Z0-9]{1,2}[0-9][A-Z]{1,3}/[A-Z0-9]{2,}$)");
    return std::regex_match(keyword, prefix) || std::regex_match(keyword, suffix);
}

bool Message::is_grid_square(const std::string& keyword) const {
    std::regex grid(R"(^[A-R]{2}[0-9]{2}$)");
    return std::regex_match(keyword, grid);
}

bool Message::is_grid_6square(const std::string& keyword) const {
    std::regex grid(R"(^[A-R]{2}[0-9]{2}[A-X]{2}$)");
    return std::regex_match(keyword, grid);
}
