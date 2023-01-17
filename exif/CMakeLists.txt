#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "exif"
)
SET(_version
    "0.6.16"
)

SET(_sources
    exif-byte-order.c
    exif-loader.c
    exif-mnote-data.c
    exif-content.c
    exif-log.c
    exif-tag.c
    exif-data.c
    exif-mem.c
    exif-utils.c
    exif-entry.c
    exif-mnote-data-canon.c
    mnote-canon-entry.c
    exif-format.c
    exif-mnote-data-olympus.c
    mnote-canon-tag.c
    exif-ifd.c
    exif-mnote-data-pentax.c
    mnote-olympus-entry.c
    mnote-pentax-tag.c
    mnote-pentax-entry.c
    mnote-olympus-tag.c
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libexif/pentax ${CMAKE_CURRENT_SOURCE_DIR}/libexif/olympus ${CMAKE_CURRENT_SOURCE_DIR}/libexif/canon
          ${CMAKE_CURRENT_SOURCE_DIR}/private
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE -DGETTEXT_PACKAGE="libexif-12"
          -DHAVE_DLFCN_H=1
          -DHAVE_INTTYPES_H=1
          -DHAVE_MEMORY_H=1
          -DHAVE_STDLIB_H=1
          -DHAVE_STRINGS_H=1
          -DHAVE_STRING_H=1
          -DHAVE_SYS_STAT_H=1
          -DHAVE_SYS_TYPES_H=1
          -DHAVE_UNISTD_H=1
          -DICONV_CONST=const
          -DPACKAGE="libexif"
          -DPACKAGE_BUGREPORT="libexif-devel@lists.sourceforge.net"
          -DPACKAGE_NAME="EXIF"
          -DPACKAGE_STRING="EXIF"
          -DPACKAGE_TARNAME="libexif"
          -DPACKAGE_VERSION=${_version}
          -DSTDC_HEADERS=1
          -DVERSION=${_version}
)
IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-Wno-conversion" "-Wno-tautological-compare"
  )
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DEXIF_BUILD=1"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
