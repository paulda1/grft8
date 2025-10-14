/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_MESSAGE_PROD_IMPL_H
#define INCLUDED_FT8_MESSAGE_PROD_IMPL_H

#include <gnuradio/ft8/message_prod.h>

namespace gr {
  namespace ft8 {

    class Generate_message : public message_prod
    {
     private:

     public:
      Generate_message();
      ~Generate_message();

      void process_input(const pmt::pmt_t& msg);
    };

  } // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_MESSAGE_PROD_IMPL_H */



// // Where all the action really happens
// void forecast (int noutput_items, gr_vector_int &ninput_items_required);

// int general_work(int noutput_items,
//      gr_vector_int &ninput_items,
//      gr_vector_const_void_star &input_items,
//      gr_vector_void_star &output_items);
