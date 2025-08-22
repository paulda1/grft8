/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_MESSAGE_PROD_H
#define INCLUDED_FT8_MESSAGE_PROD_H

#include <gnuradio/ft8/api.h>
#include <gnuradio/block.h>
#include <string>

namespace gr {
  namespace ft8 {

    /*!
     * \brief <+description of block+>
     * \ingroup ft8
     *
     */
    class FT8_API message_prod : virtual public gr::block
    {
     public:
      typedef std::shared_ptr<message_prod> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of ft8::message_prod.
       *
       * To avoid accidental use of raw pointers, ft8::message_prod's
       * constructor is in a private implementation
       * class. ft8::message_prod::make is the public interface for
       * creating new instances.
       */
      static sptr make();
    };

  } // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_MESSAGE_PROD_H */
