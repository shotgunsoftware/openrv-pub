#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "freetype"
)

# QT on Linux is linking with the system freetype.  We must do the same in order not to violate ODR.
IF(RV_TARGET_LINUX)
  FIND_PACKAGE(Freetype REQUIRED)
  ADD_LIBRARY(${_target} ALIAS Freetype::Freetype)
ELSE()
  LIST(
    APPEND
    _sources
    autofit/autofit.c
    base/ftbase.c
    base/ftbbox.c
    base/ftbdf.c
    base/ftbitmap.c
    base/ftdebug.c
    base/ftgasp.c
    base/ftglyph.c
    base/ftgxval.c
    base/ftinit.c
    base/ftlcdfil.c
    base/ftmm.c
    base/ftotval.c
    base/ftpatent.c
    base/ftpfr.c
    base/ftstroke.c
    base/ftsynth.c
    base/ftsystem.c
    base/fttype1.c
    base/ftwinfnt.c
    base/ftxf86.c
    bdf/bdf.c
    cache/ftcache.c
    cff/cff.c
    cid/type1cid.c
    gzip/ftgzip.c
    lzw/ftlzw.c
    pcf/pcf.c
    pfr/pfr.c
    psaux/psaux.c
    pshinter/pshinter.c
    psnames/psnames.c
    raster/raster.c
    sfnt/sfnt.c
    smooth/smooth.c
    truetype/truetype.c
    type1/type1.c
    type42/type42.c
    winfonts/winfnt.c
  )

  ADD_LIBRARY(
    ${_target} STATIC
    ${_sources}
  )

  TARGET_INCLUDE_DIRECTORIES(
    ${_target}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/builds/unix
  )

  TARGET_LINK_LIBRARIES(${_target} ZLIB::ZLIB)

  IF(RV_TARGET_DARWIN)
    TARGET_COMPILE_OPTIONS(
      ${_target}
      PRIVATE "-DDARWIN_NO_CARBON"
    )
  ENDIF()

  IF(RV_TARGET_WINDOWS)
    TARGET_COMPILE_OPTIONS(
      ${_target}
      PRIVATE "-D_LIB"
    )
  ENDIF()

  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DFT2_BUILD_LIBRARY" "-DFT_CONFIG_CONFIG_H=<ftconfig.h>" "-DFT_CONFIG_MODULES_H=<ftmodule.h>" "-DFT_CONFIG_OPTION_SYSTEM_ZLIB"
  )

  RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
ENDIF()
