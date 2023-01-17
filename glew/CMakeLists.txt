#
# Copyright (C) 2023  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "glew"
)

SET(_sources
    glew.c
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC . ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DGLEW_STATIC"
)
IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-Wno-pointer-sign"
  )
ENDIF()

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
