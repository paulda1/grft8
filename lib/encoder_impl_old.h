/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_ENCODER_IMPL_H
#define INCLUDED_FT8_ENCODER_IMPL_H

#include "message.h"
#include <algorithm>
#include <bitset>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/ft8/encoder.h>
#include <string>
#include <vector>

namespace gr {
namespace ft8 {

class encoder_impl : public encoder {
   private:
    message d_message_obj;
    std::vector<float> d_waveform;
    size_t d_sample_idx;
    bool d_waveform_generated;

    static std::vector<float> s_gaussian_coefs;
    static bool s_coefs_initialized;
    gr::filter::fir_filter_fff::sptr d_fir_filter;

    void generate_waveform();
    void init_gaussian_coefs();
    std::vector<float> generate_rectangular_fsk(const std::vector<int>& symbols);
    std::vector<float> apply_fir_filter(const std::vector<float>& fsk_signal);

   public:
    encoder_impl(std::string message_text);
    ~encoder_impl();

    message::message_type get_message_type();
    const std::string& get_processed_message();

    int work(int noutput_items, gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_ENCODER_IMPL_H */
