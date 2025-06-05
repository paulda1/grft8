/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_ENCODER_IMPL_H
#define INCLUDED_FT8_ENCODER_IMPL_H

#include <gnuradio/ft8/encoder.h>
#include <string>

namespace gr {
  namespace ft8 {

    class encoder_impl : public encoder
    {
     private:
        std::string d_message;
     public:
      encoder_impl(std::string message);
      ~encoder_impl();

      // Where all the action really happens
      int work(
              int noutput_items,
              gr_vector_const_void_star &input_items,
              gr_vector_void_star &output_items
      );
    };

  } // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_ENCODER_IMPL_H */
