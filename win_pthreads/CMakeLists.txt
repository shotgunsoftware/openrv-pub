#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "win_pthreads"
)

LIST(APPEND _sources pthread.c)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DPTW32_BUILD" "-wd4028"
)

TARGET_LINK_LIBRARIES(${_target} WS2_32)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
