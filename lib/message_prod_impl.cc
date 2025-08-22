/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/io_signature.h>
#include "message_prod_impl.h"
#include "ft8_encoder.h"
#include "message.h"
#include <string>
#include <memory>

namespace gr {
  namespace ft8 {

    using input_type = pmt::pmt_t;
    using output_type = pmt::pmt_t;
    message_prod::sptr
    message_prod::make()
    {
      return gnuradio::make_block_sptr<message_prod_impl>();
    }

    message_prod_impl::message_prod_impl()
      : gr::block("FT8_message_processing",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0))
    {
        message_port_register_in(pmt::mp("Input"));
        message_port_register_out(pmt::mp("Output"));
        
        set_msg_handler(pmt::mp("Input"),     //use process_input directly                  
            [this](const pmt::pmt_t& msg) { process_input(msg);});
    }

    message_prod_impl::~message_prod_impl()
    {
    }

    float 
    message_prod_impl::generate_gaussian_pulse_taps(float t) 
    {
        float pulse;
        
        // Gaussian pulse equation: p(t) = (1/2T) * [erf(kBT(t+0.5)/T) - erf(kBT(t-0.5)/T)]
        float k = M_PI * std::sqrt(2.0f / std::log(2.0f));
        float bt = 2.0f;
        float erf_coeff = k * bt;
        float T = 0.160f;
        float norm = 1.0f / (2.0f * T); 

        float erf_plus = std::erf(erf_coeff * (t/T + 0.5f));
        float erf_minus = std::erf(erf_coeff * (t/T - 0.5f));
        pulse = norm * (erf_plus - erf_minus);
        
        return pulse;
    }

    std::vector<float> 
    message_prod_impl::fsk_tones(std::vector<int> symbols)
    {
      const int sample_rate = 48000;
      const float baud_rate = 6.25f; //79*0.160
      const float freq_shift = 6.25f;
      const float carrier_frequency = 1500.0f;
      const float amplitude = 1.0f;

      int samples_per_symbol = static_cast<int>(sample_rate / baud_rate);  //7680
      
      d_logger->info("Samples per symbol calculation: {} / {} = {} samples", 
                     sample_rate, baud_rate, samples_per_symbol);
      d_logger->info("Input symbols: {}, Expected output size: {}", 
                     symbols.size(), symbols.size() * samples_per_symbol);
      
      size_t expected_size = symbols.size() * samples_per_symbol;
      std::vector<float> fsk_signal;
      fsk_signal.reserve(expected_size);
      


      for (size_t i = 0; i < symbols.size(); ++i) {
        float symbol_frequency = carrier_frequency + static_cast<float>(symbols[i])*freq_shift;
        for (auto j = 0; j < samples_per_symbol; ++j){
          int sample_index = i * samples_per_symbol + j;
          float t = static_cast<float>(sample_index) / sample_rate;
          fsk_signal.push_back(amplitude*std::cos(2*M_PI*symbol_frequency*t + generate_gaussian_pulse_taps(t)));
        }
      }

      //phase += h*symbols[i]/period;
      d_logger->info("Generated FSK signal: {} samples (expected {})", 
                     fsk_signal.size(), expected_size);
      
      if (fsk_signal.size() != expected_size) {
          d_logger->error("SIZE MISMATCH: Generated {} samples, expected {}", 
                         fsk_signal.size(), expected_size);
      }
      
      d_logger->info("FSK signal generated successfully");
      return fsk_signal;
    }
        
    void 
    message_prod_impl::process_input(const pmt::pmt_t& msg)
    {
        std::string input_message;

        if (pmt::is_symbol(msg)){
          input_message = pmt::symbol_to_string(msg);
        } 

        message message_obj = message(input_message);
        d_logger->info("Starting message processing...");

        ft8_encoder encoder;
        std::bitset<77> message_bits = encoder.encode_free_text(message_obj);
        d_logger->info("Message bits: {}", message_bits.to_string());

        std::bitset<91> crc = encoder.calc_crc(message_bits);
        d_logger->info("CRC bits: {}", crc.to_string());

        std::bitset<174> ldpc = encoder.apply_ldpc(crc);
        std::vector<int> symbols = encoder.bits_to_fsk8(ldpc);

        std::string symbol_str = "";
        for (size_t i = 0; i< symbols.size(); ++i){
          if(i>0) symbol_str += ", ";
          symbol_str += std::to_string(symbols[i]);
        }
        d_logger->info("Generated symbols: {}", symbol_str);
        
        std::vector<float> fsk_tone_vals;
        fsk_tone_vals = fsk_tones(symbols);

        pmt::pmt_t symbol_pmt = pmt::init_f32vector(fsk_tone_vals.size(), fsk_tone_vals.data());

        // pmt::pmt_t symbol_pmt = pmt::init_s32vector(symbols.size(), symbols.data());
        pmt::pmt_t meta = pmt::make_dict();
        meta = pmt::dict_add(meta, pmt::string_to_symbol("packet_len"), pmt::from_long(fsk_tone_vals.size()));
        
        pmt::print(meta);
        pmt::print(symbol_pmt);
        pmt::pmt_t output_pdu = pmt::cons(meta, symbol_pmt);
        pmt::print(output_pdu);
        message_port_pub(pmt::mp("Output"), output_pdu);
    }
  } /* namespace ft8 */
} /* namespace gr */

 // void
    // message_prod_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    // {
    // #pragma message("implement a forecast that fills in how many items on each input you need to produce noutput_items and remove this warning")
    //   /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
    // }

    // int
    // message_prod_impl::general_work (int noutput_items,
    //                    gr_vector_int &ninput_items,
    //                    gr_vector_const_void_star &input_items,
    //                    gr_vector_void_star &output_items)
    // {
    //   auto in = static_cast<const input_type*>(input_items[0]);
    //   auto out = static_cast<output_type*>(output_items[0]);
        
    //   void message_port_pub(pmt::pmt_t port_id, pmt::pmt_t msg);

    //   #pragma message("Implement the signal processing in your block and remove this warning")
    //   // Do <+signal processing+>
    //   // Tell runtime system how many input items we consumed on
    //   // each input stream.
    //   consume_each (noutput_items);

    //   // Tell runtime system how many output items we produced.
    //   return noutput_items;
    // }