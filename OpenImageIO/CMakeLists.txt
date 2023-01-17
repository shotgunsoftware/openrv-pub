#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "OpenImageIO"
)

LIST(
  APPEND
  _sources
  color_ocio.cpp
  exif.cpp
  formatspec.cpp
  imagebuf.cpp
  imagebufalgo.cpp
  imagebufalgo_compare.cpp
  imagebufalgo_copy.cpp
  imagebufalgo_deep.cpp
  imagebufalgo_draw.cpp
  imagebufalgo_opencv.cpp
  imagebufalgo_pixelmath.cpp
  imagebufalgo_xform.cpp
  imagebufalgo_yee.cpp
  imageinput.cpp
  imageio.cpp
  imageioplugin.cpp
  imageoutput.cpp
  iptc.cpp
  libtexture/environment.cpp
  libtexture/imagecache.cpp
  libtexture/texoptions.cpp
  libtexture/texture3d.cpp
  libtexture/texturesys.cpp
  libutil/SHA1.cpp
  libutil/argparse.cpp
  libutil/errorhandler.cpp
  libutil/filesystem.cpp
  libutil/filter.cpp
  libutil/hashes.cpp
  libutil/paramlist.cpp
  libutil/plugin.cpp
  libutil/strutil.cpp
  libutil/sysutil.cpp
  libutil/timer.cpp
  libutil/typedesc.cpp
  libutil/ustring.cpp
  libutil/xxhash.cpp
  maketexture.cpp
  xmp.cpp
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC OpenEXR::OpenEXR Boost::thread
  PRIVATE OpenColorIO
          Boost::date_time
          Boost::filesystem
          Boost::regex
          Boost::system
          Boost::thread
          tiff
)

IF(RV_TARGET_WINDOWS)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PRIVATE Psapi
  )
ENDIF()

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DUSE_OCIO=0"
)
IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DBOOST_LIB_DIAGNOSTIC" "-DOpenImageIO_EXPORTS" "-D__TBB_NO_IMPLICIT_LINKAGE"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
