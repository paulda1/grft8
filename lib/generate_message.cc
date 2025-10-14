/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/io_signature.h>
#include "generate_message.h"
#include "encode.h"
#include "message.h"
#include "signal.h"
#include <string>
#include <iomanip>
#include <sstream>

namespace gr {
  namespace ft8 {

    using input_type = pmt::pmt_t;
    using output_type = pmt::pmt_t;
    message_prod::sptr
    message_prod::make()
    {
      return gnuradio::make_block_sptr<Generate_message>();
    }

    Generate_message::Generate_message()
      : gr::block("FT8_message_processing",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0))
    {
        message_port_register_in(pmt::mp("Input"));
        message_port_register_out(pmt::mp("Output"));
        
        set_msg_handler(pmt::mp("Input"),     //use process_input directly                  
            [this](const pmt::pmt_t& msg) { process_input(msg);});
    }

    Generate_message::~Generate_message()
    {
    }

    void 
    Generate_message::process_input(const pmt::pmt_t& msg)
    {
        std::string input_message;
        if (pmt::is_symbol(msg)){
          input_message = pmt::symbol_to_string(msg);
        } 
        Message message_obj = Message(input_message);
        d_logger->info("Starting message processing...");
        Encode encoder;
        std::bitset<77> message_bits = encoder.encode_free_text(message_obj);
        d_logger->info("Message bits: {}", message_bits.to_string());
        
        // Convert message_bits to packed hex bytes
        std::stringstream packed_hex;
        packed_hex << std::hex << std::setfill('0');
        for (int i = 0; i < 77; i += 8) {
            uint8_t byte = 0;
            for (int j = 0; j < 8 && (i + j) < 77; j++) {
                if (message_bits[i + j]) {
                    byte |= (1 << (7 - j));
                }
            }
            if (i > 0) packed_hex << " ";
            packed_hex << std::setw(2) << static_cast<int>(byte);
        }
        d_logger->info("Packed data: {}", packed_hex.str());
        
        std::bitset<91> crc = encoder.calc_crc(message_bits); // void
        std::bitset<174> ldpc = encoder.apply_ldpc(crc);
        std::vector<int> symbols = encoder.bits_to_fsk8(ldpc);
        
        // Print FSK tones as continuous string
        std::string fsk_tones_str = "";
        for (size_t i = 0; i < symbols.size(); ++i){
          fsk_tones_str += std::to_string(symbols[i]);
        }
        d_logger->info("FSK tones: {}", fsk_tones_str);
        
        Signal signal;
        std::vector<float> fsk_tone_vals;
        fsk_tone_vals = signal.fsk_tones(symbols);
        pmt::pmt_t symbol_pmt = pmt::init_f32vector(fsk_tone_vals.size(), fsk_tone_vals.data());
        pmt::pmt_t meta = pmt::make_dict();
        meta = pmt::dict_add(meta, pmt::string_to_symbol("packet_len"), pmt::from_long(fsk_tone_vals.size()));
        
        pmt::pmt_t output_pdu = pmt::cons(meta, symbol_pmt);
        message_port_pub(pmt::mp("Output"), output_pdu);
    }

  } /* namespace ft8 */
} /* namespace gr */


