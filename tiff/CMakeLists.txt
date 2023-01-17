#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "tiff"
)

SET(_sources
    tif_aux.c
    tif_close.c
    tif_codec.c
    tif_color.c
    tif_compress.c
    tif_dir.c
    tif_dirinfo.c
    tif_dirread.c
    tif_dirwrite.c
    tif_dumpmode.c
    tif_error.c
    tif_extension.c
    tif_fax3.c
    tif_fax3sm.c
    tif_flush.c
    tif_getimage.c
    tif_jpeg.c
    tif_luv.c
    tif_lzw.c
    tif_next.c
    tif_open.c
    tif_packbits.c
    tif_pixarlog.c
    tif_predict.c
    tif_print.c
    tif_read.c
    tif_strip.c
    tif_swab.c
    tif_thunder.c
    tif_tile.c
    tif_version.c
    tif_warning.c
    tif_write.c
    tif_zip.c
    tif_ojpeg.c
    tif_pixarfilm.c
)

IF(RV_TARGET_WINDOWS)
  LIST(APPEND _sources tif_win32.c)
ELSE()
  LIST(APPEND _sources tif_unix.c)
ENDIF()

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PRIVATE jpeg ZLIB::ZLIB
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DJPEG_SUPPORT=1"
)

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DTIF_PLATFORM_CONSOLE" "-DUSE_WIN32_FILEIO" "-D_LIB" "-D_POSIX_"
  )
ENDIF()

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
