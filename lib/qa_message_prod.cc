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
#include <gnuradio/logger.h>

#include <sys/stat.h>
#include <pmt/pmt.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <bitset>
#include <boost/test/unit_test.hpp>
#include <cmath>

#include <vector>
#include "encode.h"
#include "message.h"
#include "signal.h"
#include <gnuradio/ft8/encoder.h>

static gr::logger test_logger("QA");

BOOST_AUTO_TEST_CASE(test_message_prod)
{
    auto tb = gr::make_top_block("test_ft8_message_prod");

    pmt::pmt_t test_message = pmt::string_to_symbol("TADISAWESOME");

    auto msg_strobe = gr::blocks::message_strobe::make(
        test_message, 
        1000
    );

    //https://wiki.gnuradio.org/index.php/Packet_Communications
    auto msg_processor = gr::ft8::message_prod::make();
    auto pdu_to_stream = gr::pdu::pdu_to_stream_f::make(
        gr::pdu::EARLY_BURST_APPEND      
    );

    const float sample_rate = 12000.0f;

    auto wav_sink = gr::blocks::wavfile_sink::make(
        "./ft8_signal_updated.wav",
        1,        
        static_cast<unsigned int>(sample_rate),
        gr::blocks::FORMAT_WAV,         
        gr::blocks::FORMAT_PCM_16,          
        false                                  // append (false = overwrite)
    );
    
    tb->msg_connect(msg_strobe, "strobe", msg_processor, "Input");
    tb->msg_connect(msg_processor, "Output", pdu_to_stream, "pdus");
    tb->connect(pdu_to_stream, 0, wav_sink, 0);

    test_logger.info("FT8 chain connected successfully");

    tb->start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    tb->stop();
    tb->wait();

    test_logger.info("Flowgraph executed successfully");
}

BOOST_AUTO_TEST_CASE(test_free_text_encoding) {
    Message msg("TNX BOB 73 GL");
    Encode encoder;
    
    std::bitset<77> message_bits = encoder.encode_free_text(msg);
   
        
    // Print entire 77-bit sequence
    test_logger.info("Full 77-bit sequence:");
    std::string bit_string;
    for (int i = 0; i < 77; i++) {
      bit_string += (message_bits[i] ? '1' : '0');
      if ((i + 1) % 8 == 0 && i < 77) bit_string += " ";
    }
    test_logger.info("{}", bit_string);

    // Reference: 77-bit payload
    const uint8_t expected_payload[10] = {
      0x63, 0xED, 0xCE, 0xE2, 0xA4, 0xAE, 0x07, 0xF5, 0x00, 0x00
    };
    // Test 77-bit message
    for (int byte_idx = 0; byte_idx < 10; byte_idx++) {
      uint8_t actual_byte = 0;
      for (int bit = 0; bit < 8; bit++) {
        int bit_pos = byte_idx * 8 + bit;
          if (bit_pos < 77) {
              if (message_bits[bit_pos]) {
                actual_byte |= (0x80 >> bit);
              }
          }
       }
     test_logger.info("77-bit Byte {}: expected={:02X}, actual={:02X}", 
                      byte_idx, expected_payload[byte_idx], actual_byte);
     BOOST_CHECK_EQUAL(actual_byte, expected_payload[byte_idx]);
    }
    
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
    
    test_logger.info("=== Testing 14-bit CRC ===");
    
    // Print entire 91-bit sequence
    bit_string = "";
    test_logger.info("Full 91-bit sequence:");
    for (int i = 0; i < 91; i++) {
      bit_string += (crc_bits[i] ? '1' : '0');
      if ((i + 1) % 8 == 0 && i < 90) bit_string += " ";
    }
    test_logger.info("{}", bit_string);
    
    // Expected 14-bit CRC:  //from ft8_lib and the official implementation
    const char* expected_crc_bits = "11111110001011";
    
    test_logger.info("Expected CRC (14 bits): {}", expected_crc_bits);
    
    std::string actual_crc_bits;
    for (int i = 77; i < 91; i++) {
      actual_crc_bits += (crc_bits[i] ? '1' : '0');
    }
    test_logger.info("Actual CRC (14 bits):   {}", actual_crc_bits);
    
    // Test each CRC bit
    for (int i = 0; i < 14; i++) {
      int bit_pos = 77 + i;
      bool expected = (expected_crc_bits[i] == '1');
      bool actual = crc_bits[bit_pos];
      BOOST_CHECK_EQUAL(actual, expected);
    }
    
    const char* expected_ldpc_bits = "011000111110110111001110111000101010010010101110000001111111010100000000000001111111000101110101110011111010000101100110101000111011110110000100000010101111000001010000100010";
   
    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
    for (int i = 0; i < 77; i++) {
      BOOST_CHECK_EQUAL(ldpc_bits[i], message_bits[i]);  
    }

    for (int i = 77; i < 91; i++) {
      BOOST_CHECK_EQUAL(ldpc_bits[i], crc_bits[i]); 
    }

    std::string expected_str = expected_ldpc_bits;
    
    std::string actual_str;
    for (int i = 0; i < 174; ++i) {  
      actual_str += ldpc_bits[i] ? '1' : '0';
    }
    
    test_logger.info("{}", actual_str);
    
//    // Print comparison
//    for (size_t i = 0; i < expected_str.length(); i += 8) {
//      size_t chunk_size = std::min(size_t(8), expected_str.length() - i);
//      std::string expected_chunk = expected_str.substr(i, chunk_size);
//      std::string actual_chunk = actual_str.substr(i, chunk_size);
//      
//      test_logger.info("e: {}", expected_chunk);
//      test_logger.info("a: {}", actual_chunk);
//      test_logger.info("");  
//    }
    
    bit_string = "";
    test_logger.info("Full 83 parity bits:");
    for (int i = 91; i < 174; i++) {
      bit_string += (ldpc_bits[i] ? '1' : '0');
      if ((i + 1) % 8 == 0 && i < 174) bit_string += " ";
    }
    test_logger.info("{}", bit_string);

    for (int i = 0; i < 174; ++i) {
      bool expected = (expected_ldpc_bits[i] == '1');
      bool actual = ldpc_bits[i];
      BOOST_CHECK_EQUAL(actual, expected);
    }
    
    const char* expected_symbol_seq = "3140652207447147063336401773500017703140652646427306546072440503670130533140652";
    std::vector<int> symbols = encoder.bits_to_fsk8(ldpc_bits);
    
    for (int i = 0; i < 79; ++i){
      BOOST_CHECK_EQUAL(expected_symbol_seq[i] - '0', symbols[i]);
    }
}

BOOST_AUTO_TEST_CASE(test_sample_generation) {
    Message msg("TNX BOB 73 GL");
    Encode encoder;
    Signal sig_generator;

    std::bitset<77> message_bits = encoder.encode_free_text(msg);
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
    std::vector<int> symbols = encoder.bits_to_fsk8(ldpc_bits);
    std::vector<float> fsk_tone_vals = sig_generator.fsk_tones(symbols);
    BOOST_CHECK_EQUAL(fsk_tone_vals.size(), 180000);
}

BOOST_AUTO_TEST_CASE(test_symbol_conversion_syncs) {
    Message msg("TNX BOB 73 GL");
    Encode encoder;

    std::bitset<77> message_bits = encoder.encode_free_text(msg);
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
    std::vector<int> symbols = encoder.bits_to_fsk8(ldpc_bits);

    BOOST_CHECK_EQUAL(symbols.size(), 79);

    for (int symbol : symbols) {
        BOOST_CHECK(symbol >= 0 && symbol <= 7);
    }

    std::vector<int> expected_sync = {3, 1, 4, 0, 6, 5, 2};
    // Print sync at position 0-6
    std::string sync1_str = "";
    for (size_t j = 0; j < 7; ++j) {
        if (j > 0)
            sync1_str += ",";
        sync1_str += std::to_string(symbols[j]);
    }
    test_logger.info("Position 0-6:   [{}]", sync1_str);

    // Print sync at position 36-42
    std::string sync2_str = "";
    for (size_t j = 0; j < 7; ++j) {
        if (j > 0)
            sync2_str += ",";
        sync2_str += std::to_string(symbols[j + 36]);
    }
    test_logger.info("Position 36-42: [{}]", sync2_str);

    // Print sync at position 72-78
    std::string sync3_str = "";
    for (size_t j = 0; j < 7; ++j) {
        if (j > 0)
            sync3_str += ",";
        sync3_str += std::to_string(symbols[j + 72]);
    }
    test_logger.info("Position 72-78: [{}]", sync3_str);

    // Print expected sync
    std::string expected_str = "";
    for (size_t j = 0; j < 7; ++j) {
        if (j > 0)
            expected_str += ",";
        expected_str += std::to_string(expected_sync[j]);
    }
    test_logger.info("Expected sync:  [{}]", expected_str);

    for (size_t i = 0; i < 7; ++i) {
        // First sync (positions 0-6)
        BOOST_CHECK_MESSAGE(expected_sync[i] == symbols[i],
                            "Sync 1 mismatch at position " + std::to_string(i) + ": expected " +
                                std::to_string(expected_sync[i]) + " but got " +
                                std::to_string(symbols[i]));

        // Second sync (positions 36-42)
        BOOST_CHECK_MESSAGE(expected_sync[i] == symbols[i + 36],
                            "Sync 2 mismatch at position " + std::to_string(i + 36) +
                                ": expected " + std::to_string(expected_sync[i]) + " but got " +
                                std::to_string(symbols[i + 36]));

        // Third sync (positions 72-78)
        BOOST_CHECK_MESSAGE(expected_sync[i] == symbols[i + 72],
                            "Sync 3 mismatch at position " + std::to_string(i + 72) +
                                ": expected " + std::to_string(expected_sync[i]) + " but got " +
                                std::to_string(symbols[i + 72]));
    }

    test_logger.debug("Symbol conversion test passed - sync patterns correct");
}


    
//    // Test CRC calculation
//    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
//    
//    test_logger.info("=== Testing 91-bit CRC ===");
//    for (int byte_idx = 0; byte_idx < 12; byte_idx++) {
//        uint8_t actual_byte = 0;
//        for (int bit = 0; bit < 8; bit++) {
//            int bit_pos = byte_idx * 8 + bit;
//            if (bit_pos < 91) {
//                if (crc_bits[bit_pos]) {
//                    actual_byte |= (0x80 >> bit);
//                }
//            }
//        }
//        test_logger.info("CRC Byte {}: expected={:02X}, actual={:02X}", 
//                         byte_idx, expected_crc[byte_idx], actual_byte);
//        BOOST_CHECK_EQUAL(actual_byte, expected_crc[byte_idx]);
//    }
//    
//    // Reference: 174-bit LDPC codeword
//    const uint8_t expected_ldpc[22] = {
//        0x62, 0xFF, 0x4F, 0xA2, 0x24, 0xA7, 0x06, 0xD5, 0x80, 0x06, 0xD1,
//        0x3C, 0xEE, 0xA1, 0x37, 0x82, 0xBF, 0xC6, 0x0A, 0x50, 0x50, 0xC8
//    };
//    
//    // Test LDPC encoding
//    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
//    
//    test_logger.info("=== Testing 174-bit LDPC ===");
//    for (int byte_idx = 0; byte_idx < 22; byte_idx++) {
//        uint8_t actual_byte = 0;
//        for (int bit = 0; bit < 8; bit++) {
//            int bit_pos = byte_idx * 8 + bit;
//            if (bit_pos < 174) {
//                if (ldpc_bits[bit_pos]) {
//                    actual_byte |= (0x80 >> bit);
//                }
//            }
//        }
//        test_logger.info("LDPC Byte {}: expected={:02X}, actual={:02X}", 
//                         byte_idx, expected_ldpc[byte_idx], actual_byte);
//        BOOST_CHECK_EQUAL(actual_byte, expected_ldpc[byte_idx]);
//    }
//    
//    // Test symbol generation
//    std::vector<int> symbols = encoder.bits_to_fsk8(ldpc_bits);
//    
//    const int expected_symbols[79] = {
//        3, 1, 4, 0, 6, 5, 2, 2, 0, 7, 4, 4, 7, 1, 4, 7,
//        0, 6, 3, 3, 3, 6, 4, 0, 1, 7, 7, 3, 5, 0, 0, 0,
//        1, 7, 7, 0, 3, 1, 4, 0, 6, 5, 2, 6, 4, 6, 4, 2,
//        7, 3, 0, 6, 5, 4, 6, 0, 7, 2, 4, 4, 0, 5, 0, 3,
//        6, 7, 0, 1, 3, 0, 5, 3, 3, 1, 4, 0, 6, 5, 2
//    };
//    
//    test_logger.info("=== Testing Symbols ===");
//    BOOST_REQUIRE_EQUAL(symbols.size(), 79);
//    for (int i = 0; i < 79; i++) {
//        if (symbols[i] != expected_symbols[i]) {
//            test_logger.error("Symbol mismatch at position {}: expected={}, actual={}", 
//                             i, expected_symbols[i], symbols[i]);
//        }
//        BOOST_CHECK_EQUAL(symbols[i], expected_symbols[i]);
//    }


// Function to perform parity check: H × codeword^T = 0
//std::vector<int> perform_parity_check(const std::vector<std::vector<int>>& H,
//                                      const std::bitset<174>& codeword) {
//    std::vector<int> parity_results(83, 0);
//
//    // Matrix multiplication
//    for (int row = 0; row < 83; row++) {
//        int sum = 0;
//        for (int col = 0; col < 174; col++) {
//            if (H[row][col] == 1 && codeword[col] == 1) {
//                sum ^= 1; // XOR operation for GF(2)
//            }
//        }
//        parity_results[row] = sum;
//    }
//
//    return parity_results;
//}
//
//BOOST_AUTO_TEST_CASE(test_ldpc_parity_check_validation) {
//    test_logger.info("Starting LDPC parity check validation test");
//
//    // Load the parity check matrix
//    auto H = load_parity_check_matrix("parity.dat");
//
//    message msg("TNX BOB 73 GL");
//    ft8_encoder encoder;
//
//    // Encode the complete LDPC codeword
//    std::bitset<77> message_bits = encoder.encode_free_text(msg);
//    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
//    std::bitset<174> ldpc_codeword = encoder.apply_ldpc(crc_bits);
//
//    test_logger.info("Testing message: '{}'", msg.get_message());
//    test_logger.info("77-bit message encoded, CRC applied, LDPC encoded to 174 bits");
//
//    // Perform parity check
//    std::vector<int> parity_results = perform_parity_check(H, ldpc_codeword);
//
//    // Check if parity results is all zeros (valid codeword)
//    bool is_valid = true;
//    int non_zero_count = 0;
//
//    for (int i = 0; i < 83; i++) {
//        if (parity_results[i] != 0) {
//            is_valid = false;
//            non_zero_count++;
//        }
//    }
//
//    test_logger.info("Parity check results:");
//    test_logger.info("- Parity check results length: {}", parity_results.size());
//    test_logger.info("- Non-zero parity check results elements: {}", non_zero_count);
//    test_logger.info("- Codeword valid: {}", is_valid ? "YES" : "NO");
//
//    BOOST_CHECK_MESSAGE(is_valid, "LDPC parity check failed - parity check results have " +
//                                      std::to_string(non_zero_count) + " non-zero elements");
//}
//
//BOOST_AUTO_TEST_CASE(test_basic_message_encoding) {
//    message msg("TNX BOB 73 GL");
//    ft8_encoder encoder;
//
//    std::bitset<77> message_bits = encoder.encode_free_text(msg);
//    BOOST_CHECK_EQUAL(message_bits.size(), 77);
//
//    bool has_bits_set = false;
//    for (size_t i = 0; i < 77; ++i) {
//        if (message_bits[i]) {
//            has_bits_set = true;
//            break;
//        }
//    }
//    BOOST_CHECK(has_bits_set);
//
//    test_logger.debug("Basic message encoding test passed");
//}
//
//BOOST_AUTO_TEST_CASE(test_crc_calculation) {
//    message msg("TNX BOB 73 GL");
//    ft8_encoder encoder;
//
//    std::bitset<77> message_bits = encoder.encode_free_text(msg);
//    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
//
//    BOOST_CHECK_EQUAL(crc_bits.size(), 91);
//
//    for (size_t i = 0; i < 77; ++i) {
//        BOOST_CHECK_EQUAL(message_bits[i], crc_bits[i]);
//    }
//
//    bool has_crc_bits = false;
//    for (size_t i = 77; i < 91; ++i) {
//        if (crc_bits[i]) {
//            has_crc_bits = true;
//            break;
//        }
//    }
//    BOOST_CHECK(has_crc_bits);
//
//    test_logger.debug("CRC calculation test passed");
//}
//
//BOOST_AUTO_TEST_CASE(test_ldpc_encoding) {
//    message msg("TNX BOB 73 GL");
//    ft8_encoder encoder;
//
//    std::bitset<77> message_bits = encoder.encode_free_text(msg);
//    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
//    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
//
//    BOOST_CHECK_EQUAL(ldpc_bits.size(), 174);
//
//    for (size_t i = 0; i < 91; ++i) {
//        BOOST_CHECK_EQUAL(crc_bits[i], ldpc_bits[i]);
//    }
//
//    bool has_parity_bits = false;
//    for (size_t i = 91; i < 174; ++i) {
//        if (ldpc_bits[i]) {
//            has_parity_bits = true;
//            break;
//        }
//    }
//    BOOST_CHECK(has_parity_bits);
//
//    test_logger.debug("LDPC encoding test passed");
//}


// BOOST_AUTO_TEST_CASE(test_message_type_detection) {
//     test_logger.info("Testing message type detection");
    
//     // Test standard message
//     message std_msg("CQ K1ABC FN42");
//     BOOST_CHECK_EQUAL(std_msg.message_type_detection(), message::message_type::standard);
    
//     // Test field day message
//     message fd_msg("K1ABC W1AW 1A CT");
//     BOOST_CHECK_EQUAL(fd_msg.message_type_detection(), message::message_type::field_day);
    
//     // Test DXpedition message
//     message dx_msg("K1ABC VP8STI +05");
//     BOOST_CHECK_EQUAL(dx_msg.message_type_detection(), message::message_type::dxpedition);
    
//     // Test free text message
//     message free_msg("HELLO WORLD");
//     BOOST_CHECK_EQUAL(free_msg.message_type_detection(), message::message_type::free_text);
    
//     test_logger.debug("Message type detection test passed");
// }

// BOOST_AUTO_TEST_CASE(test_various_message_types) {
//     test_logger.info("Testing encoding of various message types");
    
//     ft8_encoder encoder;
    
//     // Test standard message
//     message std_msg("CQ K1ABC FN42");
//     std::bitset<77> std_bits = encoder.encode_standard(std_msg);
//     BOOST_CHECK_EQUAL(std_bits.size(), 77);
    
//     // Test field day message
//     message fd_msg("K1ABC W1AW 1A CT");
//     std::bitset<77> fd_bits = encoder.encode_field_day(fd_msg);
//     BOOST_CHECK_EQUAL(fd_bits.size(), 77);
    
//     // Test free text message
//     message free_msg("HELLO WORLD");
//     std::bitset<77> free_bits = encoder.encode_free_text(free_msg);
//     BOOST_CHECK_EQUAL(free_bits.size(), 77);
    
//     // Test telemetry message
//     message telem_msg("1234567890ABCDEF12");
//     std::bitset<77> telem_bits = encoder.encode_telemetry(telem_msg);
//     BOOST_CHECK_EQUAL(telem_bits.size(), 77);
    
//     test_logger.debug("Various message types encoding test passed");
// }

//BOOST_AUTO_TEST_CASE(test_complete_waveform_generation_integration) {
//    test_logger.info("Testing complete waveform generation with GNU Radio integration");
//    
//    std::string test_message = "CQ K1ABC FN42";
//    
//    // Test if encoder can be created (this tests GNU Radio integration)
//    try {
//        auto encoder = gr::ft8::encoder::make(test_message);
//        test_logger.info("Created GNU Radio encoder instance with message: {}", test_message);
//        BOOST_CHECK(encoder != nullptr);
//        
//        test_logger.info("GNU Radio encoder integration test passed");
//    } catch (const std::exception& e) {
//        test_logger.error("GNU Radio encoder creation failed: {}", e.what());
//        // If GNU Radio encoder is not available, fall back to basic test
//        BOOST_WARN_MESSAGE(false, "GNU Radio encoder not available, skipping integration test");
//    }
//    
//    // Test basic encoding chain without GNU Radio
//    message msg(test_message);
//    ft8_encoder encoder;
//    
//    std::bitset<77> message_bits = encoder.encode_standard(msg);
//    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
//    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);
//    std::vector<int> symbols = encoder.bits_to_fsk8(ldpc_bits);
//    
//    BOOST_CHECK_EQUAL(symbols.size(), 79);
//    
//    // Verify we have reasonable symbol values
//    for (int symbol : symbols) {
//        BOOST_CHECK(symbol >= 0 && symbol <= 7);
//    }
//    
//    test_logger.info("Basic encoding chain test passed: {} symbols generated", symbols.size());
//}
//
//// BOOST_AUTO_TEST_CASE(test_callsign_parsing) {
////     test_logger.info("Testing callsign parsing and validation");
//    
////     message parser;
//    
////     // Valid callsigns
//     BOOST_CHECK(parser.is_callsign("K1ABC"));
//     BOOST_CHECK(parser.is_callsign("W1AW"));
//     BOOST_CHECK(parser.is_callsign("VE3XYZ"));
//     BOOST_CHECK(parser.is_callsign("G0ABC"));
    
//     // Invalid callsigns
//     BOOST_CHECK(!parser.is_callsign("123ABC"));
//     BOOST_CHECK(!parser.is_callsign("TOOLONG"));
//     BOOST_CHECK(!parser.is_callsign("AB"));
    
//     // Non-standard callsigns
//     BOOST_CHECK(parser.is_nonstd_callsign("VP8/K1ABC"));
//     BOOST_CHECK(parser.is_nonstd_callsign("K1ABC/MM"));
//     BOOST_CHECK(!parser.is_nonstd_callsign("K1ABC"));
    
//     test_logger.debug("Callsign parsing test passed");
// }

// BOOST_AUTO_TEST_CASE(test_grid_square_parsing) {
//     test_logger.info("Testing grid square parsing");
    
//     message parser;
    
//     // Valid 4-character grid squares
//     BOOST_CHECK(parser.is_grid_square("FN42"));
//     BOOST_CHECK(parser.is_grid_square("EM13"));
//     BOOST_CHECK(parser.is_grid_square("JO32"));
    
//     // Valid 6-character grid squares
//     BOOST_CHECK(parser.is_grid_6square("FN42AB"));
//     BOOST_CHECK(parser.is_grid_6square("EM13XY"));
    
//     // Invalid grid squares
//     BOOST_CHECK(!parser.is_grid_square("FZ42")); // Invalid first letter
//     BOOST_CHECK(!parser.is_grid_square("FNA2")); // Invalid third character
//     BOOST_CHECK(!parser.is_grid_square("FN4"));  // Too short
    
//     test_logger.debug("Grid square parsing test passed");
// }
////
//std::vector<std::vector<int>> 
//load_parity_check_matrix(const std::string& filename) {
//    std::vector<std::vector<int>> H(83, std::vector<int>(174, 0));
//    std::ifstream file(filename);
//
//    if (!file.is_open()) {
//        test_logger.error("Cannot open parity check file: {}", filename);
//        return H;
//    }
//
//    std::string line;
//    int col = 0;
//
//    while (std::getline(file, line) && col < 174) {
//        // Skip empty lines and comments
//        if (line.empty() || line.find("file specifies") != std::string::npos ||
//            line.find("matrix") != std::string::npos || line.find("ones") != std::string::npos) {
//            continue;
//        }
//
//        std::istringstream iss(line);
//        std::vector<int> row_indices;
//        int index;
//
//        // Parse up to 3 integers from the line
//        while (iss >> index && row_indices.size() < 3) {
//            row_indices.push_back(index);
//        }
//
//        if (row_indices.size() == 3) {
//            for (int idx : row_indices) {
//                int row = idx - 1;
//                if (row >= 0 && row < 83) {
//                    H[row][col] = 1;
//                }
//            }
//            col++;
//        }
//    }
//
//    test_logger.info("Loaded parity check matrix: {} columns processed", col);
//    return H;
//}


