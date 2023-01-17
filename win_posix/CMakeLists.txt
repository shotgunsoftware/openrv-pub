#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "win_posix"
)

LIST(APPEND _sources dirent.c dlfcn.c fnmatch.c)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DWIN_POSIX_BUILD"
)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
