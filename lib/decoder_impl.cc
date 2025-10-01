/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
//
//#include "decoder_impl.h"
//#include <gnuradio/io_signature.h>
//#include <deque>
//
//namespace gr {
//  namespace ft8 {
//
//  using input_type = float;
//  using output_type = pmt::pmt_t;
//
//  decoder::sptr 
//  decoder::make() 
//  { 
//    return gnuradio::make_block_sptr<decoder_impl>(); 
//  }
//
//  decoder_impl::decoder_impl()
//    : gr::block("decoder",
//            gr::io_signature::make(1, 1, sizeof(input_type)),
//            gr::io_signature::make(0, 0, sizeof(output_type)))
//  {
//    d_buffer.resize(BUFFER_SIZE, 0.0f);
//    message_port_register_out(pmt::mp("decoded_output"));
//  }
//
//  std::vector<float>
//  decoder_impl::get_buffer() const{
//    return std::vector<float>(d_buffer.begin(), d_buffer.end());
//  }
//
//  void 
//  decoder_impl::maintain_buffer(const std::vector<float>& samples){
//    for (float sample: samples){
//      d_buffer.push_back(sample);
//    }
//
//    while (d_buffer.size() > BUFFER_SIZE){
//      d_buffer.pop_front();
//    }
//  }
//
//  void 
//  decoder_impl::process_audio(const std::vector<float>& samples){
//    maintain_buffer(samples);
//
//    d_samples_new += samples.size();
//    if (d_samples_new >= INTERVAL_TO_PROCESS){
//      process_buffer();
//      d_samples_new = 0;
//    } 
//  }
//
//  void 
//  decoder_impl::search_freqs(){
//    for (int freq_bin = MIN_FREQ_BIN; freq_bin <= MAX_FREQ_BIN; freq_bin +=1){
//      std::vector<uint8_t> symbols = extract_symbols(freq_bin);
//    }
//  }
//
//  void 
//  decoder_impl::process_buffer(){
//    if (d_buffer.size() < BUFFER_SIZE) return;
//
//    std::vector<float> current_audio = get_buffer();
//    //detecting possibile audio signals (implement)
//    std::copy(d_buffer.begin(), d_buffer.end(), d_processing_buffer.begin());
//  }
//
//  decoder_impl::~decoder_impl() {}
//
//  int decoder_impl::work(int noutput_items,
//    gr_vector_int& ninput_items,
//    gr_vector_const_void_star& input_items,
//    gr_vector_void_star& output_items)
//  {
//      auto in = static_cast<const input_type*>(input_items[0]);
//      std::vector<float> samples(in, in + noutput_items);
//      process_audio(samples);
//
//      consume_each(noutput_items);
//      return 0;
//  }
//
//  } /* namespace ft8 */
//} /* namespace gr */
//
//// void decoder_impl::forecast(int noutput_items, gr_vector_int& ninput_items_required)
//// {
//// #pragma message( 
////     "implement a forecast that fills in how many items on each input you need to produce noutput_items and remove this warning")
////     /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
//// }
