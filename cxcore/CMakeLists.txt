#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "cxcore"
)

LIST(
  APPEND
  _sources
  cxalloc.cpp
  cxarithm.cpp
  cxarray.cpp
  cxcmp.cpp
  cxconvert.cpp
  cxcopy.cpp
  cxdatastructs.cpp
  cxdrawing.cpp
  cxdxt.cpp
  cxerror.cpp
  cximage.cpp
  cxjacobieigens.cpp
  cxlogic.cpp
  cxlut.cpp
  cxmathfuncs.cpp
  cxmatmul.cpp
  cxmatrix.cpp
  cxmean.cpp
  cxmeansdv.cpp
  cxminmaxloc.cpp
  cxnorm.cpp
  cxouttext.cpp
  cxpersistence.cpp
  cxprecomp.cpp
  cxrand.cpp
  cxsumpixels.cpp
  cxsvd.cpp
  cxswitcher.cpp
  cxtables.cpp
  cxutils.cpp
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

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DHAVE_CONFIG_H"
)

IF(RV_TARGET_DARWIN)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-fno-inline"
  )
ENDIF()

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DCVAPI_EXPORTS=1"
  )
ENDIF()

IF(RV_TARGET_LINUX)
  SET(THREADS_PREFER_PTHREAD_FLAG
      TRUE
  )
  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PUBLIC Threads::Threads
  )
ELSEIF(RV_TARGET_WINDOWS)
  TARGET_LINK_LIBRARIES(
    ${_target}
    PUBLIC win_pthreads win_posix
  )
ENDIF()

TARGET_LINK_LIBRARIES(
  ${_target}
  PRIVATE ${CMAKE_DL_LIBS}
)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
