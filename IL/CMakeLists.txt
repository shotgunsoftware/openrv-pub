#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "IL"
)

FILE(GLOB _sources *.c)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/IL
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DIL_NO_PNG" "-DIL_NO_JPG" "-DIL_NO_TIF" "-DIL_NO_MNG" "-DIL_NO_LCMS" "-DNOINLINE"
)
IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-Wno-sizeof-array-argument" "-Wno-sizeof-pointer-memaccess" "-Wno-switch"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
