# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT ft8_sources)
    message(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT ft8_sources)

########################################################################
# Check for pygccxml
########################################################################
gr_python_check_module_raw("pygccxml" "import pygccxml" PYGCCXML_FOUND)

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND ft8_python_files
    encoder_python.cc python_bindings.cc)

gr_pybind_make_oot(ft8 ../../.. gr::ft8 "${ft8_python_files}")

target_include_directories(ft8_python PUBLIC
    ${CMAKE_SOURCE_DIR}/include/gnuradio
)

# copy bindings extension for use in QA test module
add_custom_command(
    TARGET ft8_python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ft8_python>
            ${PROJECT_BINARY_DIR}/test_modules/gnuradio/ft8/)

install(
    TARGETS ft8_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/ft8
    COMPONENT pythonapi)
