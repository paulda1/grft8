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

    void generate_waveform();
    std::vector<float> generate_freq_deviation(const std::vector<int>& symbols);

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
