#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "minizip"
)

LIST(APPEND _sources ioapi.c unzip.c)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC ZLIB::ZLIB
)

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DMINIZ_DLL" "-DMINIZ_INTERNAL"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
