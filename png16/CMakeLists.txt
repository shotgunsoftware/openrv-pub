#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "png16"
)

SET(_sources
    png.c
    pngset.c
    pngget.c
    pngrutil.c
    pngtrans.c
    pngwutil.c
    pngread.c
    pngrio.c
    pngwio.c
    pngwrite.c
    pngrtran.c
    pngwtran.c
    pngmem.c
    pngerror.c
    pngpread.c
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

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC ZLIB::ZLIB
)

IF(RV_TARGET_LINUX)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DHAVE_CONFIG_H"
  )
ENDIF()

IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DHAVE_CONFIG_H"
  )
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DPNG_BUILD_DLL=1" "-DPNG_NO_MMX_CODE" "-DPNG_NO_MODULEDEF"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
