/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/io_signature.h>
#include "encoder_impl.h"

#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <map>
#include <cctype>

namespace gr {
  namespace ft8 {

    #pragma message("set the following appropriately and remove this warning")
    using output_type = float;
    encoder::sptr
    encoder::make(std::string message)
    {
      return gnuradio::make_block_sptr<encoder_impl>(message);
    }


    /*
     * The private constructor
     */
    encoder_impl::encoder_impl(std::string message)
      : gr::sync_block("encoder",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */, sizeof(output_type))),
    d_message(message)
    {
        preprocess_message();
    }

    /*
     * Our virtual destructor.
     */
    encoder_impl::~encoder_impl()
    {
    }

    void encoder_impl::preprocess_message()
    {
        input_validation();
        message_type_detection();
        d_logger->info("Preprocessed message: {}", d_message);	
    }

    void encoder_impl::input_validation()
    {
        if (d_message.empty()){
            d_logger->error("No message input");
            return;
        }

        trim();
        character_validation();
    }

    void encoder_impl::trim()
    //remove leading and trailing whitespace
    {
        size_t start = 0;
        while(start < d_message.length() && std::isspace(d_message[start])){
            start++;
        }

        size_t end = d_message.length();
        while(end > start && std::isspace(d_message[end-1])){
            end--;
        }

        d_message = d_message.substr(start, end - start);
    }
    
    void encoder_impl::character_validation()
    //only A-Z, 0-9, and / is allowed, no more than one consecutive space
    {
        size_t i = 0;
        bool last_was_space = false;
        bool last_was_slash = false;
        for (size_t j = 0; j < d_message.length(); ++j){
            char c = d_message[j];
            
            if (std::isalpha(c)){
                c = std::toupper(c);
            }
            if (!(std::isalpha(c) || std::isdigit(c) || c == ' ')){
                d_logger->error("Invalid character: {}", c);
                return;
            }
            if (!(last_was_space && c == ' ') && c != "/"){
                d_message[i] = c;
                i++;
            }   
            last_was_space = (c == ' ');
            last_was_slash = (c == "/");
        }

        d_message.resize(i); 
    }
    
    void encoder_impl::message_type_detection() //***add in UPPER LIMITS**
    //Keyword detection
    {
        std::istringstream stream(d_message);
        std::string input;
        std::vector<std::string> keywords;
        
        while(stream >> input){
            keywords.push_back(input);
        }
        
       
        //message types
        enum class message_type{
            free_text, //0.0
            dxpedition, //0.1
            field_day, //0.3
            field_dayx, //0.4
            telemetry, //0.5
            standard, //1.0
            euvhf, //2.0
            rtty_ru, //3.0
            nonstd_call, //4.0
            euvhfx, //5.0
            unknown
        } 
        
        message_type current_type = message_type::unknown;

        //if (keyword == "CQ" || keyword == "DE" || keyword == "QRZ"){}
        //***Standard and euvhf regular seem the same!! ***
   
        if (is_dxpedition(keywords)){
            current_type = message_type::dxpedition;
        }
        else if (is_telemetry(keywords)){
            current_type = message_type::telemetry;
        }
        else if (is_field_day(keywords, true)){ //more restricted due to R, so check first
            current_type = message_type::field_dayx;
        }        
        else if (is_field_day(keywords, false)){
            current_type = message_type::field_day;
        }
        else if (is_std(keywords)){
            current_type = message_type::standard;
        }
        else if (is_rtty_ru(keywords)){
            current_type = message_type::rtty_ru;
        }
        else if (is_euvhfx(keywords)){
            current_type = message_type::euvhfx;
        }
        else if (is_nonstd(keywords)){
            current_type = message_type::nonstd_call;
        }      
    }
    
    bool encoder_impl::is_nonstd(const std::vector <std::string>& keywords)
    {   
        has_nonstd = false;
        for (const auto& keyword : keywords){
            if (is_nonstd_callsign(keyword)){
               has_nonstd = true;
            }
        }
        return has_nonstd;
    } 

    bool encoder_impl::is_euvhfx(const std::vector <std::string>& keywords)
    {
        has_callsigns = false;
        has_extended_grid = false;
        
        for (const auto& keyword : keywords){
            if (is_callsign(keyword)){
                has_callsigns = true;
            }
            else if (is_grid_6square(keyword)){
                has_extended_grid = true;
            }
        }
        return has_callsigns && has_extended_grid;
    }
    
    bool encoder_impl::is_rtty_ru(const std::vector<std::string>& keywords){
        bool has_callsigns = false;
        bool has_contest = false;
        
        for (const auto& keyword : keywords){
            if (is_callsign(keyword)){
                has_callsigns = true;
            }
            else if (is_contest(keyword)){
                has_contest = true;
            }
        }
        return has_callsigns && has_contest;
    }
    
    bool encoder_impl::is_contest(const std::string& keyword){
        std::regex contest(R"(^[0-9]{3}$)");
        return std::regex_match(keyword, contest);
    }
    
    bool encoder_impl::is_std(const std::vector<std::string>& keywords){
        bool has_callsigns = false;
        bool has_grid = false;
        
        for (const auto& keyword : keywords){
            if (is_callsign(keyword)){
                has_callsigns = true;
            }
            if (is_grid_square(keyword)){
                has_grid = true;
            }
        }
        return has_callsigns && has_grid;
    }

    bool encoder_impl::is_field_day(const std::vector<std::string>& keywords, bool check_r)
    {   
        bool has_callsigns = false;
        bool has_field_day_class = false;
        bool has_r = false;

        for (const auto& keyword : keywords){
            if (is_field_day_class(keyword)){
                has_field_day_class = true;
            }
            else if (keyword == "R"){
                has_r = true;
            }
            else if (is_callsign(keyword)){
                has_callsigns = true;
            }
        }
        
        if (!has_callsigns || !has_field_day_class){
            return false;
        }
        if (check_r && !has_r){
            return false;
        }
        return true;
    }
    
    bool encoder_impl::is_field_day_class(const std::string& keyword)
    {
        std::regex fdclass(R"(^\d+[A-Z]$)");
        return std::regex_match(keyword, fdclass);
    }

    bool encoder_impl::is_telemetry(const std::vector<std::string>& keywords)
    {
        if (keywords.size == 1 && is_hex(keywords[0])){
            return true;
        }
        return false;
    }

    bool encoder_impl::is_hex(const std::string& keyword)
    {
        for (char c : keyword){
            if (!std::isxdigit(c)) return false;
        }
        return true;
    }

    bool encoder_impl::is_dxpedition(const std::vector<std::string>& keywords)
    {
        for (const auto& keyword : keywords){
            if (keyword == "RRR" || keyword == "RR73" || keyword == "73" || is_signal_report(keyword)){
                return true;
            }
            else if (is_callsign(keyword)){
                d_logger->info("Callsign detected: {}", keyword);
            }
        }
        return false;
    }

    bool encoder_impl::is_signal_report(const std::string& keyword)
    {
        //+-nn
        if (keyword.size() == 3){
            static const std::regex sreport(R"(^[+-]\d{2}$)");
            return std::regex_match(keyword, sreport);        
        }
        return false;
    }
    
    bool encoder_impl::is_callsign(const std::string& keyword)
    {
        //one-two character prefix, at least one is a letter
        //then a decimal digit, and a suffix up to three letters
    
        static const std::regex callsign(R"(^[A-Z0-9]{1,2}[0-9][A-Z]{1,3}(?:/[A-Z0-9]{1,4})$)");
        return std::regex_match(keyword, callsign);
    }
    
    bool encoder_impl::is_nonstd_callsign(const std::string& keyword)
    {
        static const std::regex prefix(R"(^[A-Z0-9]{2,4}/[A-Z0-9]{1,2}[A-Z]{1,3}$)");
        static const std::regex suffix(R"(^[A-Z0-9]{1,2}[0-9][A-Z]{1,3}/[A-Z0-9]{2,}$)");
        return std::regex_match(keyword, prefix) || std::regex_match(keyword, suffix);
    }
    
    bool encoder_impl::is_grid_square(const std::string& keyword)
    {
        std::regex grid(R"(^[A-R]{2}[0-9]{2}$)");
        return std::regex_match(keyword, grid);
    }
    
    bool encoder_impl::is_grid_6square(const std::string& keyword)
    {
        std::regex grid(R"(^[A-R]{2}[0-9]{2}[A-X]{2}$)");
        return std::regex_match(keyword, grid);
    }

    int
    encoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      auto out = static_cast<output_type*>(output_items[0]);

      #pragma message("Implement the signal processing in your block and remove this warning")
      // Do <+signal processing+>

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace ft8 */
} /* namespace gr */
