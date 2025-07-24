/* -*- c++ -*- */
/*
 * Copyright 2025 Daniel Paul.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/logger.h>

#include <algorithm>
#include <bitset>
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "encoder_impl.h"
#include "ft8_encoder.h"
#include "message.h"

static gr::logger test_logger("FT8_QA");

std::vector<std::vector<int>> load_parity_check_matrix(const std::string& filename) {
    std::vector<std::vector<int>> H(83, std::vector<int>(174, 0));
    std::ifstream file(filename);

    if (!file.is_open()) {
        test_logger.error("Cannot open parity check file: {}", filename);
        return H;
    }

    std::string line;
    int col = 0;

    while (std::getline(file, line) && col < 174) {
        // Skip empty lines and comments
        if (line.empty() || line.find("file specifies") != std::string::npos ||
            line.find("matrix") != std::string::npos || line.find("ones") != std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        std::vector<int> row_indices;
        int index;

        // Parse up to 3 integers from the line
        while (iss >> index && row_indices.size() < 3) {
            row_indices.push_back(index);
        }

        if (row_indices.size() == 3) {
            for (int idx : row_indices) {
                int row = idx - 1;
                if (row >= 0 && row < 83) {
                    H[row][col] = 1;
                }
            }
            col++;
        }
    }

    test_logger.info("Loaded parity check matrix: {} columns processed", col);
    return H;
}

// Function to perform parity check: H × codeword^T = 0
std::vector<int> perform_parity_check(const std::vector<std::vector<int>>& H,
                                      const std::bitset<174>& codeword) {
    std::vector<int> parity_results(83, 0);

    // Matrix multiplication
    for (int row = 0; row < 83; row++) {
        int sum = 0;
        for (int col = 0; col < 174; col++) {
            if (H[row][col] == 1 && codeword[col] == 1) {
                sum ^= 1; // XOR operation for GF(2)
            }
        }
        parity_results[row] = sum;
    }

    return parity_results;
}

BOOST_AUTO_TEST_CASE(test_ldpc_parity_check_validation) {
    test_logger.info("Starting LDPC parity check validation test");

    // Load the parity check matrix
    auto H = load_parity_check_matrix("parity.dat");

    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    // Encode the complete LDPC codeword
    std::bitset<77> message_bits = encoder.encode_standard(msg);
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
    std::bitset<174> ldpc_codeword = encoder.apply_ldpc(crc_bits);

    test_logger.info("Testing message: '{}'", msg.get_message());
    test_logger.info("77-bit message encoded, CRC applied, LDPC encoded to 174 bits");

    // Perform parity check
    std::vector<int> parity_results = perform_parity_check(H, ldpc_codeword);

    // Check if parity results is all zeros (valid codeword)
    bool is_valid = true;
    int non_zero_count = 0;

    for (int i = 0; i < 83; i++) {
        if (parity_results[i] != 0) {
            is_valid = false;
            non_zero_count++;
        }
    }

    test_logger.info("Parity check results:");
    test_logger.info("- Parity check results length: {}", parity_results.size());
    test_logger.info("- Non-zero parity check results elements: {}", non_zero_count);
    test_logger.info("- Codeword valid: {}", is_valid ? "YES" : "NO");

    BOOST_CHECK_MESSAGE(is_valid, "LDPC parity check failed - parity check results have " +
                                      std::to_string(non_zero_count) + " non-zero elements");
}

BOOST_AUTO_TEST_CASE(test_basic_message_encoding) {
    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    std::bitset<77> message_bits = encoder.encode_standard(msg);
    BOOST_CHECK_EQUAL(message_bits.size(), 77);

    bool has_bits_set = false;
    for (size_t i = 0; i < 77; ++i) {
        if (message_bits[i]) {
            has_bits_set = true;
            break;
        }
    }
    BOOST_CHECK(has_bits_set);

    test_logger.debug("Basic message encoding test passed");
}

BOOST_AUTO_TEST_CASE(test_crc_calculation) {
    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    std::bitset<77> message_bits = encoder.encode_standard(msg);
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);

    BOOST_CHECK_EQUAL(crc_bits.size(), 91);

    for (size_t i = 0; i < 77; ++i) {
        BOOST_CHECK_EQUAL(message_bits[i], crc_bits[i]);
    }

    bool has_crc_bits = false;
    for (size_t i = 77; i < 91; ++i) {
        if (crc_bits[i]) {
            has_crc_bits = true;
            break;
        }
    }
    BOOST_CHECK(has_crc_bits);

    test_logger.debug("CRC calculation test passed");
}

BOOST_AUTO_TEST_CASE(test_ldpc_encoding) {
    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    std::bitset<77> message_bits = encoder.encode_standard(msg);
    std::bitset<91> crc_bits = encoder.calc_crc(message_bits);
    std::bitset<174> ldpc_bits = encoder.apply_ldpc(crc_bits);

    BOOST_CHECK_EQUAL(ldpc_bits.size(), 174);

    for (size_t i = 0; i < 91; ++i) {
        BOOST_CHECK_EQUAL(crc_bits[i], ldpc_bits[i]);
    }

    bool has_parity_bits = false;
    for (size_t i = 91; i < 174; ++i) {
        if (ldpc_bits[i]) {
            has_parity_bits = true;
            break;
        }
    }
    BOOST_CHECK(has_parity_bits);

    test_logger.debug("LDPC encoding test passed");
}

BOOST_AUTO_TEST_CASE(test_symbol_conversion) {
    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    std::bitset<77> message_bits = encoder.encode_standard(msg);
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

    // Test each sync position separately with detailed error messages
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

BOOST_AUTO_TEST_CASE(test_complete_waveform_generation) {
    message msg("CQ K1ABC FN42");
    ft8_encoder encoder;

    // std::bitset<77> message_bits = encoder.encode_standard(msg);
    // std::vector<float> waveform = encoder.encode_ft8_complete(message_bits);

    // int expected_samples = 79 * (48000 / 6.25);
    // BOOST_CHECK_EQUAL(waveform.size(), expected_samples);

    // bool has_signal = false;
    // for (float sample : waveform) {
    //   if (std::abs(sample) > 0.001f) {
    //     has_signal = true;
    //     break;
    //   }
    // }
    // BOOST_CHECK(has_signal);

    // auto minmax = std::minmax_element(waveform.begin(), waveform.end());
    // BOOST_CHECK(*minmax.first >= -2.0f);
    // BOOST_CHECK(*minmax.second <= 2.0f);

    // test_logger.debug(
    //     "Complete waveform generation test passed: {} samples, range [{}, {}]",
    //     waveform.size(), *minmax.first, *minmax.second);
}
