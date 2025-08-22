/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_FT8_DECODER_H
#define INCLUDED_FT8_DECODER_H

#include <gnuradio/block.h>
#include <gnuradio/ft8/api.h>

namespace gr {
namespace ft8 {

/*!
 * \brief <+description of block+>
 * \ingroup ft8
 *
 */
class FT8_API decoder : virtual public gr::block
{
public:
    typedef std::shared_ptr<decoder> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of ft8::decoder.
     *
     * To avoid accidental use of raw pointers, ft8::decoder's
     * constructor is in a private implementation
     * class. ft8::decoder::make is the public interface for
     * creating new instances.
     */
    static sptr make();
};

} // namespace ft8
} // namespace gr

#endif /* INCLUDED_FT8_DECODER_H */
