#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "openjpeg"
)

LIST(
  APPEND
  _sources
  bio.c
  cidx_manager.c
  cio.c
  dwt.c
  event.c
  image.c
  j2k.c
  j2k_lib.c
  jp2.c
  jpt.c
  mct.c
  mqc.c
  openjpeg.c
  phix_manager.c
  pi.c
  ppix_manager.c
  raw.c
  t1.c
  t2.c
  tcd.c
  tgt.c
  thix_manager.c
  tpix_manager.c
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-O3"
  )
ENDIF()

IF(RV_TARGET_LINUX)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-O3"
  )
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DOPJ_EXPORTS=1"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
