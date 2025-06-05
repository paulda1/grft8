find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_FT8 gnuradio-ft8)

FIND_PATH(
    GR_FT8_INCLUDE_DIRS
    NAMES gnuradio/ft8/api.h
    HINTS $ENV{FT8_DIR}/include
        ${PC_FT8_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_FT8_LIBRARIES
    NAMES gnuradio-ft8
    HINTS $ENV{FT8_DIR}/lib
        ${PC_FT8_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-ft8Target.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_FT8 DEFAULT_MSG GR_FT8_LIBRARIES GR_FT8_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_FT8_LIBRARIES GR_FT8_INCLUDE_DIRS)
