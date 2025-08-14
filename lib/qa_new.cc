#include <gnuradio/hier_block2.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/block.h>
#include <gnuradio/top_block.h>
#include <gnuradio/logger.h>
#include <gnuradio/ft8/message_prod.h>
#include <gnuradio/blocks/message_strobe.h>
#include <gnuradio/blocks/message_debug.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/add_const_ff.h>
#include <gnuradio/blocks/vector_source.h>
#include <gnuradio/blocks/vector_sink.h>
#include <gnuradio/blocks/file_sink.h>
#include <gnuradio/blocks/vco_f.h>
#include <gnuradio/blocks/repeat.h>
#include <gnuradio/blocks/wavfile_sink.h>
#include <gnuradio/filter/interp_fir_filter.h>
#include <gnuradio/pdu/pdu_to_stream.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <pmt/pmt.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>
#include <cmath>

//*todo: remember to move this function somewhere else:
static gr::logger d_logger("FT8_QA");

std::vector<float> generate_gaussian_pulse_taps(int samples_per_symbol, float bt) {
    int pulse_len = 3 * samples_per_symbol;
    std::vector<float> pulse(pulse_len);
    
    // Gaussian pulse equation: p(t) = (1/2T) * [erf(kBT(t+0.5)/T) - erf(kBT(t-0.5)/T)]
    float k = M_PI *std::sqrt(2.0f / std::log(2.0f));
    float erf_coeff = k * bt;
    float T = 0.160;
    float norm = 1/(2*T); 

    for (int i = 0; i < pulse_len; ++i) {
        float t = (i - pulse_len/2.0f)/samples_per_symbol; //center it
        float erf_plus = std::erf(erf_coeff * (t/T + 0.5f));
        float erf_minus = std::erf(erf_coeff * (t/T - 0.5f));
        pulse[i] = norm * (erf_plus - erf_minus);
    }
    
    return pulse;
}


BOOST_AUTO_TEST_CASE(test_message_prod_basic)
{
    auto tb = gr::make_top_block("test_ft8_message_prod");

    pmt::pmt_t test_message = pmt::string_to_symbol("VE4ABCW9XYZER");

    auto msg_strobe = gr::blocks::message_strobe::make(
        test_message, 
        10000
    );
    //https://wiki.gnuradio.org/index.php/Packet_Communications
    auto msg_processor = gr::ft8::message_prod::make();
    // auto msg_debug = gr::blocks::message_debug::make();
    auto pdu_to_stream = gr::pdu::pdu_to_stream_f::make(
        gr::pdu::EARLY_BURST_APPEND      
    );

    const float sample_rate = 48000.0f;
    const float baud_rate = 6.25f;
    const float gaussian_bt = 2.0f;
    const float vco_sensitivity = (2 * M_PI)/ sample_rate;
    const int samples_per_symbol = static_cast<int>(sample_rate/baud_rate);

    std::vector<float> gaussian_taps = generate_gaussian_pulse_taps(samples_per_symbol, gaussian_bt);

    auto gaussian_filter = gr::filter::interp_fir_filter_fff::make(samples_per_symbol, gaussian_taps);
    auto vco = gr::blocks::vco_f::make(sample_rate, vco_sensitivity, 1);
    auto vector_sink = gr::blocks::vector_sink_f::make();
    auto debug_sink_4 = gr::blocks::file_sink::make(sizeof(float), "./debug_final_vco.dat");

    auto wav_sink = gr::blocks::wavfile_sink::make(
        "./ft8_signal.wav",
        1,        
        static_cast<unsigned int>(sample_rate),
        gr::blocks::FORMAT_WAV,         
        gr::blocks::FORMAT_PCM_16,          
        false                                  // append (false = overwrite)
    );

    tb->msg_connect(msg_strobe, "strobe", msg_processor, "Input");
    tb->msg_connect(msg_processor, "Output", pdu_to_stream, "pdus");

    tb->connect(pdu_to_stream, 0, gaussian_filter, 0);
    tb->connect(gaussian_filter, 0, vco, 0);
    tb->connect(vco, 0, wav_sink, 0);
    tb->connect(vco, 0, debug_sink_4, 0);     // Should be symbol values 0-7


    d_logger.info("FT8 chain connected successfully");

    tb->start();
    std::this_thread::sleep_for(std::chrono::seconds(15));
    tb->stop();
    tb->wait();
    d_logger.info("Flowgraph executed successfully");

}
    // const float freq_shift = 6.25f; 

    // auto add_base_freq = gr::blocks::add_const_ff::make(base_freq);
    // const float max_freq = base_freq + (max_symbol * freq_shift); // 1000 + 7*6.25 = 1043.75 Hz
    // const float max_symbol = 7.0f;

    // auto taps_minmax = std::minmax_element(gaussian_taps.begin(), gaussian_taps.end());
    // d_logger.info("Gaussian taps range: min={:.6f}, max={:.6f}", *taps_minmax.first, *taps_minmax.second);
    // auto symbol_to_freq = gr::blocks::multiply_const_ff::make(6.25); //<float>?

    //samples per symbol: 48000/6.25

    // tb->connect(gaussian_filter, 0, add_base_freq, 0);
    // tb->msg_connect(msg_processor, "Output", pdu_to_stream, "print");
    // auto messages = msg_debug->get_message(0);

    // tb->connect(gaussian_filter, 0, symbol_to_freq, 0);
    // tb->connect(symbol_to_freq, 0, vco, 0);

    // tb->connect(vco, 0, vector_sink, 0);
    // tb->connect(vco, 0, debug_sink_4, 0);

    // tb->connect(gaussian_filter, 0, debug_sink_1, 0);     // Should be 0, 6.25, 12.5, 18.75, etc.

    // auto output_data = vector_sink->data();
    // d_logger.info("Generated {} samples of FT8 signal", output_data.size());

    // tb->connect(add_base_freq, 0, debug_sink_3, 0);      // Should be 1000, 1006.25, 1012.5, 1018.75, etc.
    // tb->connect(vco, 0, debug_sink_4, 0);

  // auto file_sink = gr::blocks::file_sink::make(sizeof(float), "./ft8_modulated_signal.dat");
    // auto debug_sink_0 = gr::blocks::file_sink::make(sizeof(float), "./debug_after_pdu.dat");
    // auto debug_sink_1 = gr::blocks::file_sink::make(sizeof(float), "./debug_after_gaussian.dat");
    // auto debug_sink_2 = gr::blocks::file_sink::make(sizeof(float), "./debug_after_multiply.dat");  
    // auto debug_sink_3 = gr::blocks::file_sink::make(sizeof(float), "./debug_after_add.dat");

    // // tb->connect(gaussian_filter, 0, add_base_freq, 0);
    // tb->connect(gaussian_filter, 0, symbol_to_freq, 0);
    // // tb->connect(symbol_to_freq, 0, add_base_freq, 0);
    // // tb->connect(add_base_freq, 0, vco, 0);
    // // tb->connect(add_base_freq, 0, vco, 0);
    // tb->connect(symbol_to_freq, 0, vco, 0);

    
    // if (!output_data.empty()) {
    //     d_logger.info("First 10 samples:");
    //     size_t samples_to_show = std::min(static_cast<size_t>(10), output_data.size());
    //     for (size_t i = 0; i < samples_to_show; ++i) {
    //         d_logger.info("  Sample[{}]: {:.6f}", i, output_data[i]);
    //     }
        
    //     auto minmax = std::minmax_element(output_data.begin(), output_data.end());
    //     d_logger.info("Sample range: min={:.6f}, max={:.6f}", *minmax.first, *minmax.second);
        
    //     bool all_zero = std::all_of(output_data.begin(), output_data.end(), 
    //                                [](float x) { return x == 0.0f; });
    //     if (all_zero) {
    //         d_logger.warn("All samples are zero!");
    //     }
    // }