#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "cv"
)

LIST(
  APPEND
  _sources
  cvaccum.cpp
  cvadapthresh.cpp
  cvapprox.cpp
  cvcalccontrasthistogram.cpp
  cvcalcimagehomography.cpp
  cvcalibinit.cpp
  cvcalibration.cpp
  cvcamshift.cpp
  cvcanny.cpp
  cvcolor.cpp
  cvcondens.cpp
  cvcontours.cpp
  cvcontourtree.cpp
  cvconvhull.cpp
  cvcorner.cpp
  cvcornersubpix.cpp
  cvderiv.cpp
  cvdistransform.cpp
  cvdominants.cpp
  cvemd.cpp
  cvfeatureselect.cpp
  cvfilter.cpp
  cvfloodfill.cpp
  cvfundam.cpp
  cvgeometry.cpp
  cvhaar.cpp
  cvhistogram.cpp
  cvhough.cpp
  cvimgwarp.cpp
  cvinpaint.cpp
  cvkalman.cpp
  cvlinefit.cpp
  cvlkpyramid.cpp
  cvmatchcontours.cpp
  cvmoments.cpp
  cvmorph.cpp
  cvmotempl.cpp
  cvoptflowbm.cpp
  cvoptflowhs.cpp
  cvoptflowlk.cpp
  cvpgh.cpp
  cvposit.cpp
  cvprecomp.cpp
  cvpyramids.cpp
  cvpyrsegmentation.cpp
  cvrotcalipers.cpp
  cvsamplers.cpp
  cvsegmentation.cpp
  cvshapedescr.cpp
  cvsmooth.cpp
  cvsnakes.cpp
  cvsubdivision2d.cpp
  cvsumpixels.cpp
  cvswitcher.cpp
  cvtables.cpp
  cvtemplmatch.cpp
  cvthresh.cpp
  cvundistort.cpp
  cvutils.cpp
  dummy.cpp
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC cxcore
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DHAVE_CONFIG_H"
)

IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-Wno-sizeof-pointer-memaccess" "-fno-inline"
  )
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DCVAPI_EXPORTS=1"
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
