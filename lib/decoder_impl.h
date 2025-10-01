/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
//
//#ifndef INCLUDED_FT8_DECODER_IMPL_H
//#define INCLUDED_FT8_DECODER_IMPL_H
//
//#include <gnuradio/ft8/decoder.h>
//
//namespace gr {
//  namespace ft8 {
//
//  class decoder_impl : public decoder
//  {
//  private:
//    std::deque<float> d_buffer;
//    std::vector<float> d_processing_buffer;
//    size_t d_samples_new = 0;
//    static constexpr size_t SAMPLE_RATE = 48000;
//    static constexpr size_t BUFFER_SIZE = 768000; //16 seconds*48000
//    static constexpr size_t INTERVAL_TO_PROCESS = 24000; //0.5 * 48000
//    //0.16 is period, so a window as close as possible to this period
//    //48000*0.16 = 7680, closest power of 2 is 8192 (2^13) so choose that as FFT size
//    static constexpr size_t FFT_SIZE = 8192;
//    static constexpr double FREQ_RESOLUTION = SAMPLE_RATE/FFT_SIZE;
//    //I'll have to change these two values if it's going to be a general frequency transmitter in the future
//    static constexpr size_t MIN_FREQ_BIN = 250; //250*5.86 = 1465 (around 1500 Hz)
//    static constexpr size_t MAX_FREQ_BIN = 270; //270*5.86 = 1582.2 (should capture upper part as well)
//
//  public:
//      decoder_impl();
//      ~decoder_impl();
//
//      void maintain_buffer(const std::vector<float>& samples);
//      void process_audio(const std::vector<float>& samples);
//      void process_buffer();
//      std::vector<float> get_buffer() const;
//
//      int work(int noutput_items,
//        gr_vector_int& ninput_items,
//        gr_vector_const_void_star& input_items,
//        gr_vector_void_star& output_items);
//  };
//
//  } // namespace ft8
//} // namespace gr
//
//#endif /* INCLUDED_FT8_DECODER_IMPL_H */


// // Where all the action really happens
// void forecast(int noutput_items, gr_vector_int& ninput_items_required);
