/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SIGNAL_H
#define SIGNAL_H

#include <vector>
#include <gnuradio/logger.h>
#include <gnuradio/ft8/api.h>

namespace gr {
class logger;
}

class FT8_API Signal {
   public:
    Signal();
    
    std::vector<float> fsk_tones(std::vector<int> symbols);
    float generate_gaussian_pulse_taps(float t);

   private:
    gr::logger d_logger; 
};

#endif
