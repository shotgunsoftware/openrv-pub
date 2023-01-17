#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "yaml"
)

SET(_sources
    api.c
    dumper.c
    emitter.c
    loader.c
    parser.c
    reader.c
    scanner.c
    writer.c
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

IF(RV_TARGET_LINUX)
  TARGET_LINK_LIBRARIES(${_target})
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_LINK_LIBRARIES(${_target})
ENDIF()

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DHAVE_CONFIG_H"
)
IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DUSE_" "-DYAML_DECLARE_EXPORT"
  )
ENDIF()

IF(RV_TARGET_LINUX)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DUSE_NEDALLOC"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
