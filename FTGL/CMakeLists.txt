#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "FTGL"
)

SET(_sources
    FTBitmapFont.cpp
    FTExtrudeGlyph.cpp
    FTLayoutGlue.cpp
    FTPolygonGlyph.cpp
    FTBitmapGlyph.cpp
    FTFace.cpp
    FTLibrary.cpp
    FTSimpleLayout.cpp
    FTBuffer.cpp
    FTFont.cpp
    FTOutlineFont.cpp
    FTSize.cpp
    FTBufferFont.cpp
    FTFontGlue.cpp
    FTOutlineGlyph.cpp
    FTTextureFont.cpp
    FTBufferGlyph.cpp
    FTGlyph.cpp
    FTPixmapFont.cpp
    FTTextureGlyph.cpp
    FTCharmap.cpp
    FTGlyphContainer.cpp
    FTPixmapGlyph.cpp
    FTVectoriser.cpp
    FTContour.cpp
    FTGlyphGlue.cpp
    FTPoint.cpp
    FTExtrudeFont.cpp
    FTLayout.cpp
    FTPolygonFont.cpp
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

# Our custom build of freetype has the ft2build header in the root of the include path whereas the system one has it inside the freetype2 subdir, so if we're
# using a system freetype, make sure to add the ft2build to the include path.
IF(RV_TARGET_LINUX)
  SET(_system_freetype_inc_path
      ${FREETYPE_INCLUDE_DIR_ft2build}
  )
ENDIF()

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${_system_freetype_inc_path}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private ${CMAKE_CURRENT_SOURCE_DIR}/FTLayout ${CMAKE_CURRENT_SOURCE_DIR}/FTGlyph ${CMAKE_CURRENT_SOURCE_DIR}/FTFont
)

IF(RV_TARGET_LINUX)
  SET(THREADS_PREFER_PTHREAD_FLAG
      TRUE
  )
  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PRIVATE Threads::Threads
  )
ENDIF()

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC freetype
)

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DFTGL_LIBRARY_STATIC" "-wd4355"
  )
  TARGET_LINK_LIBRARIES(
    ${_target}
    PRIVATE win_pthreads
  )
ENDIF()

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
