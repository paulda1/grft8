/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_ENCODER_H
#define INCLUDED_FT8_ENCODER_H

#include <gnuradio/ft8/api.h>
#include <gnuradio/sync_block.h>
#include <string>

namespace gr {
  namespace ft8 {

    /*!
     * \brief <+description of block+>
     * \ingroup ft8
     *
     */
    class FT8_API encoder : virtual public gr::sync_block
    {
     public:
      typedef std::shared_ptr<encoder> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of ft8::encoder.
       *
       * To avoid accidental use of raw pointers, ft8::encoder's
       * constructor is in a private implementation
       * class. ft8::encoder::make is the public interface for
       * creating new instances.
       */
      static sptr make(std::string message="CQ DX VE4ABC EN19");
    };

  } // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_ENCODER_H */
