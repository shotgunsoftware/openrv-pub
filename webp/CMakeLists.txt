#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "webp"
)

LIST(
  APPEND
  _sources
  dec/alpha.c
  dec/buffer.c
  dec/frame.c
  dec/idec.c
  dec/io.c
  dec/layer.c
  dec/quant.c
  dec/tree.c
  dec/vp8.c
  dec/webp.c
  dsp/cpu.c
  dsp/dec.c
  dsp/dec_neon.c
  dsp/dec_sse2.c
  dsp/enc.c
  dsp/enc_sse2.c
  dsp/upsampling.c
  dsp/upsampling_sse2.c
  dsp/yuv.c
  enc/alpha.c
  enc/analysis.c
  enc/config.c
  enc/cost.c
  enc/filter.c
  enc/frame.c
  enc/iterator.c
  enc/layer.c
  enc/picture.c
  enc/quant.c
  enc/syntax.c
  enc/tree.c
  enc/webpenc.c
  mux/muxedit.c
  mux/muxinternal.c
  mux/muxread.c
  utils/alpha.c
  utils/bit_reader.c
  utils/bit_writer.c
  utils/filters.c
  utils/quant_levels.c
  utils/tcoder.c
  utils/thread.c
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

IF(RV_TARGET_LINUX)
  SET(THREADS_PREFER_PTHREAD_FLAG
      TRUE
  )
  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PUBLIC Threads::Threads
  )
ELSEIF(RV_TARGET_WINDOWS)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PUBLIC win_pthreads win_posix
  )
ENDIF()

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DHAVE_CONFIG_H"
)

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
