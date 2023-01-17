#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "LibRaw"
)

LIST(
  APPEND
  _sources
  internal/dcraw_common.cpp
  internal/dcraw_fileio.cpp
  internal/demosaic_packs.cpp
  libraw_c_api.cpp
  libraw_cxx.cpp
  libraw_datastream.cpp
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

SET_TARGET_PROPERTIES(
  ${_target}
  PROPERTIES OUTPUT_NAME raw
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_COMPILE_DEFINITIONS(
  ${_target}
  PRIVATE PACKAGE_NAME=\"LibRaw\"
          PACKAGE_TARNAME=\"libraw\"
          PACKAGE_VERSION=\"0.18.0\"
          PACKAGE_STRING=\"LibRaw\ 0.18.0\"
          PACKAGE_BUGREPORT=\"\"
          STDC_HEADERS=1
          HAVE_SYS_TYPES_H=1
          HAVE_SYS_STAT_H=1
          HAVE_STDLIB_H=1
          HAVE_STRING_H=1
          HAVE_MEMORY_H=1
          HAVE_STRINGS_H=1
          HAVE_INTTYPES_H=1
          HAVE_STDINT_H=1
          USE_LCMS2
)
IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_DEFINITIONS(
    ${_target}
    PRIVATE LIBRAW_BUILDLIB
  )
ELSE()
  TARGET_COMPILE_DEFINITIONS(
    ${_target}
    PRIVATE HAVE_UNISTD_H=1 HAVE_DLFCN_H=1
  )
ENDIF()

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC lcms ZLIB::ZLIB Boost::thread
)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
