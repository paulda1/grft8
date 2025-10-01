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
#include <algorithm>
#include <numeric>


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
        //float norm = 1.0f / (2.0f * T); 

        float erf_plus = std::erf(erf_coeff * (t/T + 0.5f));
        float erf_minus = std::erf(erf_coeff * (t/T - 0.5f));
        //pulse = norm * (erf_plus - erf_minus);
        pulse = (erf_plus - erf_minus);        
        return pulse;
    }

    std::vector<float> 
    message_prod_impl::fsk_tones(std::vector<int> symbols)
    {
      const int sample_rate = 48000;
      const float T = 0.160f;
      const float baud_rate = 6.25f; //79*0.160
      const float freq_shift = 6.25f;
      const float carrier_frequency = 1500.0f;
      const float amplitude = 0.5f;

      int samples_per_symbol = static_cast<int>(sample_rate / baud_rate);  //7680
      
      d_logger->info("Samples per symbol calculation: {} / {} = {} samples", 
                     sample_rate, baud_rate, samples_per_symbol);
      d_logger->info("Input symbols: {}, Expected output size: {}", 
                     symbols.size(), symbols.size() * samples_per_symbol);
      
      //generate a gaussian pulse over time range capturing entire pulse
      const int pulse_span_symbols = 4;
      const int pulse_length = pulse_span_symbols * samples_per_symbol;
      std::vector<float> d_gaussian_pulse(pulse_length);
      
      float start_time = -2.0f*T;

      for (int i = 0; i < pulse_length; ++i){
        float t = start_time + (static_cast<float>(i)/sample_rate);
        d_gaussian_pulse[i] = generate_gaussian_pulse_taps(t);
      }
      
      float pulse_max = *std::max_element(d_gaussian_pulse.begin(), d_gaussian_pulse.end());
      for (int i = 0; i < pulse_length; ++i){
        d_gaussian_pulse[i] = d_gaussian_pulse[i] / pulse_max;
      }
      
      float normalized_max = *std::max_element(d_gaussian_pulse.begin(), d_gaussian_pulse.end());
      d_logger->info("Gaussian pulse peak AFTER normalization: {}", normalized_max);


      //superposition
      int signal_length = symbols.size() * samples_per_symbol;
      std::vector<float> freq_deviation(signal_length, 0.0f);
      int pulse_center = pulse_length/2;

      for (size_t n = 0; n < symbols.size(); ++n){
        float bn = static_cast<float>(symbols[n]);
        int symbol_start = n * samples_per_symbol;

        for (int i = 0; i < pulse_length; ++i){
          int idx = symbol_start + i - pulse_center;

          if(idx >= 0 && idx < signal_length){
            freq_deviation[idx] += bn * d_gaussian_pulse[i] * freq_shift;
          }
        }
      }

      //integration
      std::vector<float> phase(signal_length, 0.0f);
      phase[0] = 0.0f;
      for (int i = 1; i < signal_length; ++i){
        phase[i] = phase[i-1] + 2.0f * M_PI * freq_deviation[i]/sample_rate;
      }
     
      
      //************************************
      // Statistical analysis of freq_deviation
      float min_val = *std::min_element(freq_deviation.begin(), freq_deviation.end());
      float max_val = *std::max_element(freq_deviation.begin(), freq_deviation.end());
      
      // Calculate mean
      float sum = std::accumulate(freq_deviation.begin(), freq_deviation.end(), 0.0f);
      float mean = sum / freq_deviation.size();
      
      // For quartiles, sort a copy
      std::vector<float> sorted_dev = freq_deviation;
      std::sort(sorted_dev.begin(), sorted_dev.end());
      
      size_t n = sorted_dev.size();
      float q1 = sorted_dev[n / 4];
      float median = sorted_dev[n / 2];
      float q3 = sorted_dev[3 * n / 4];
      
      d_logger->info("Freq deviation stats - Min: {}, Q1: {}, Median: {}, Q3: {}, Max: {}, Mean: {}", 
                     min_val, q1, median, q3, max_val, mean);
      
      // Count non-zero values
      int non_zero = std::count_if(freq_deviation.begin(), freq_deviation.end(), 
                                    [](float val) { return std::abs(val) > 1e-6f; });
      d_logger->info("Non-zero freq_deviation values: {} out of {}", non_zero, freq_deviation.size());
      
      
     // Statistical analysis of phase
      float phase_min = *std::min_element(phase.begin(), phase.end());
      float phase_max = *std::max_element(phase.begin(), phase.end());
      
      std::vector<float> sorted_phase = phase;
      std::sort(sorted_phase.begin(), sorted_phase.end());
      
      size_t np = sorted_phase.size();
      float phase_q1 = sorted_phase[np / 4];
      float phase_median = sorted_phase[np / 2];
      float phase_q3 = sorted_phase[3 * np / 4];
      
      d_logger->info("Phase stats - Min: {}, Q1: {}, Median: {}, Q3: {}, Max: {}", 
                     phase_min, phase_q1, phase_median, phase_q3, phase_max);
      
      // Check phase differences to see if it's varying
      float phase_diff_sum = 0.0f;
      for (int i = 1; i < std::min(1000, (int)phase.size()); ++i) {
          phase_diff_sum += std::abs(phase[i] - phase[i-1]);
      }
      d_logger->info("Average phase change over first 1000 samples: {}", phase_diff_sum / 999.0f);
            //************************************

      //signal generation
      std::vector<float> fsk_signal;
      fsk_signal.reserve(signal_length);
      
      for (int i = 0; i < signal_length; ++i){
        float t = static_cast<float>(i) / sample_rate;
        fsk_signal.push_back(amplitude* std::cos(2 * M_PI * carrier_frequency * t + phase[i]));
      } 
      
      //padding
      int samples_15_secs = 15* sample_rate;
      std::vector<float> padded_signal(samples_15_secs, 0.0f);

      for (int i = 0; i < signal_length; ++i){
        padded_signal[i] = fsk_signal[i];
      }

      //**************************************************
      // Check the actual signal values going into the WAV
      float padded_min = *std::min_element(padded_signal.begin(), padded_signal.end());
      float padded_max = *std::max_element(padded_signal.begin(), padded_signal.end());
      float padded_mean = std::accumulate(padded_signal.begin(), padded_signal.end(), 0.0f) / padded_signal.size();
      
      // Count how many samples are at the limits
      int at_max = std::count_if(padded_signal.begin(), padded_signal.end(), 
                                  [](float v) { return std::abs(v - 0.5f) < 0.001f; });
      int at_min = std::count_if(padded_signal.begin(), padded_signal.end(), 
                                  [](float v) { return std::abs(v + 0.5f) < 0.001f; });
      
      d_logger->info("Padded signal stats - Min: {}, Max: {}, Mean: {}", padded_min, padded_max, padded_mean);
      d_logger->info("Samples at limits: {} at max, {} at min, out of {} total", at_max, at_min, padded_signal.size());

          // After phase integration, calculate instantaneous frequency
      std::vector<float> inst_freq(signal_length);
      inst_freq[0] = carrier_frequency;
      for (int i = 1; i < signal_length; ++i){
          float phase_diff = phase[i] - phase[i-1];
          inst_freq[i] = carrier_frequency + (phase_diff * sample_rate) / (2.0f * M_PI);
      }
      
      // Check instantaneous frequency statistics
      float freq_min = *std::min_element(inst_freq.begin(), inst_freq.end());
      float freq_max = *std::max_element(inst_freq.begin(), inst_freq.end());
      std::vector<float> sorted_freq = inst_freq;
      std::sort(sorted_freq.begin(), sorted_freq.end());
      float freq_median = sorted_freq[signal_length/2];
      
      d_logger->info("Instantaneous frequency - Min: {} Hz, Median: {} Hz, Max: {} Hz", 
                     freq_min, freq_median, freq_max);
      d_logger->info("Expected range: {} to {} Hz", carrier_frequency, carrier_frequency + 7*freq_shift);      
                  //**************************************************
      
      d_logger->info("FSK signal generated successfully");
      return padded_signal;
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
