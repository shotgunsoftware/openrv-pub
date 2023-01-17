#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "gtoModule"
)

LIST(APPEND _sources gtoHeader.cpp gtoReader.cpp gtoWriter.cpp gtomodule.cpp)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-Wno-deprecated-writable-strings"
  )
ENDIF()

IF(RV_TARGET_LINUX)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-fno-strict-aliasing"
            "-DNDEBUG"
            "-fwrapv"
            "-Wall"
            "-Wstrict-prototypes"
            "-DHAVE_SEM_OPEN=1"
            "-DHAVE_FD_TRANSFER=1"
            "-DHAVE_SEM_TIMEDWAIT=1"
  )
ENDIF()

TARGET_LINK_LIBRARIES(
  ${_target}
  Python::Python
  Gto
  TwkUtil
  Imath::Imath
  Boost::filesystem
  Boost::system
  ZLIB::ZLIB
)

SET_TARGET_PROPERTIES(
  ${_target}
  PROPERTIES OUTPUT_NAME gto
)

# Note: Cannot use gto on Windows as the generated import lib's name clashes with the Gto lib because the windows file system is not case sensitive. As a work
# around, we use a different name for the import lib.
IF(RV_TARGET_WINDOWS)
  TARGET_LINK_OPTIONS(${_target} PUBLIC "/IMPLIB:gtoModule.lib")
ENDIF()

RV_STAGE(TYPE "PYTHON_PLUGIN" TARGET ${_target})
