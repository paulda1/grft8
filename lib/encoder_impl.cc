/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/io_signature.h>
#include "encoder_impl.h"
#include "message.h"
#include "ft8_encoder.h"

namespace gr {
  namespace ft8 {

    using output_type = float;

    encoder::sptr
    encoder::make(std::string message)
    {
      return gnuradio::make_block_sptr<encoder_impl>(message);
    }

    encoder_impl::encoder_impl(std::string message_text)
      : gr::sync_block("encoder",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */, sizeof(output_type))),
    d_message_obj(message_text),
    d_sample_idx(0),
    d_waveform_generated(false)
    {
        // Message preprocessing is handled by the message object constructor
        d_logger->info("Encoder message: {}", d_message_obj.get_message());
        generate_waveform();
    }

    encoder_impl::~encoder_impl(){}

    message::message_type
    encoder_impl::get_message_type()
    {
        return d_message_obj.message_type_detection();
    }

    const std::string&
    encoder_impl::get_processed_message()
    {
        return d_message_obj.get_message();
    }

    std::vector<float>
    encoder_impl::generate_rectangular_fsk(const std::vector<int>& symbols)
    {
      const int sample_rate = 48000;
      const float baud_rate = 6.25f;
      const float freq_shift = 6.25f;

      int samples_per_symbol = sample_rate / static_cast<int>(baud_rate);
      std::vector<float> fsk_signal(symbols.size() * samples_per_symbol, 0.0f);

      for (size_t  i = 0; i < symbols.size(); ++i){
        float freq_deviation = symbols[i] * freq_shift;

        for (int j = 0; j < samples_per_symbol; ++j){
          fsk_signal[i*samples_per_symbol + j] = freq_deviation;
        }
      }
      d_logger->info("FSK signal generated");
      return fsk_signal;
    }

    void
    encoder_impl::generate_waveform()
    {
        try {
            d_logger->info("Starting waveform generation...");

            ft8_encoder encoder;
            std::bitset<77> message_bits = encoder.encode_standard(d_message_obj);
            d_logger->info("Message bits: {}", message_bits.to_string());

            std::bitset<91> crc = encoder.calc_crc(message_bits);
            d_logger->info("CRC bits: {}", crc.to_string());

            std::bitset<174> ldpc = encoder.apply_ldpc(crc);
            std::vector<int> symbols = encoder.bits_to_fsk8(ldpc);
            d_logger->info("Generated {} symbols", symbols.size());

            std::vector<float> rectangular_fsk = generate_rectangular_fsk(symbols);
            d_logger->info("Rectangular FSK size: {}", rectangular_fsk.size());

            // Check if waveform has actual values
            if (!d_waveform.empty()) {
                auto minmax = std::minmax_element(d_waveform.begin(), d_waveform.end());
                d_logger->info("Waveform range: {} to {}", *minmax.first, *minmax.second);
            }

            d_waveform_generated = true;
        } catch (const std::exception& e) {
            d_logger->error("Exception in generate_waveform: {}", e.what());
            d_waveform_generated = false;
        }
    }

    int
    encoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      auto out = static_cast<output_type*>(output_items[0]);
      if (! d_waveform_generated || d_waveform.empty()) {
        std::fill(out, out+noutput_items, 0.0f);
        return noutput_items;
      }

      int to_output = 0;

      for(int i = 0; i<noutput_items; ++i) {
        if (d_sample_idx < d_waveform.size()){
            out[i] = d_waveform[d_sample_idx];
            d_sample_idx++;
            to_output++;
        } else {
            out[i] = 0.0f;
            to_output++;
        }
      // Tell runtime system how many output items we produced.
      }
      return to_output;
   }
  } /* namespace ft8 */
} /* namespace gr */


// std::vector<float>
// encoder_impl::apply_gaussian_filter(const std::vector<float>& fsk_signal)
// {
//   if(!s_coefs_initialized){
//     init_gaussian_coefs();
//   }

//   size_t output_size = fsk_signal.size() + s_gaussian_coefs.size() - 1;
//   std::vector<float> filtered_signal(output_size, 0.0f);
// }
