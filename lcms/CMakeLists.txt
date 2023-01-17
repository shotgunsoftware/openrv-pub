#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "lcms"
)

SET(_sources
    cmscam02.c
    cmserr.c
    cmshalf.c
    cmsio1.c
    cmsmtrx.c
    cmspack.c
    cmsps2.c
    cmstypes.c
    cmsxform.c
    cmscgats.c
    cmsgamma.c
    cmsintrp.c
    cmslut.c
    cmsnamed.c
    cmspcs.c
    cmssamp.c
    cmsvirt.c
    cmscnvrt.c
    cmsgmt.c
    cmsio0.c
    cmsmd5.c
    cmsopt.c
    cmsplugin.c
    cmssm.c
    cmswtpnt.c
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_COMPILE_DEFINITIONS(
  ${_target}
  PRIVATE PACKAGE_NAME=\"lcms2\"
          PACKAGE_TARNAME=\"lcms2\"
          PACKAGE_VERSION=\"2.6\"
          PACKAGE_STRING=\"lcms\ 2.6\"
          PACKAGE_BUGREPORT=\"\"
          STDC_HEADERS=1
          HAVE_SYS_TYPES_H=1
          HAVE_SYS_STAT_H=1
          HAVE_STDLIB_H=1
          HAVE_STRING_H=1
          HAVE_MEMORY_H=1
          HAVE_STRINGS_H=1
          HAVE_INTTYPES_H=1
          HAVE_STDINT_H=1
          HasZLIB=1
          HasTIFF=1
          HAVE_TIFFCONF_H=1
)
IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_DEFINITIONS(
    ${_target}
    PRIVATE SIZEOF_INT=4
            SIZEOF_UNSIGNED_INT=4
            SIZEOF_LONG=4
            SIZEOF_UNSIGNED_LONG=4
            SIZEOF_LONG_LONG=8
            SIZEOF_UNSIGNED_LONG_LONG=8
            CMS_DLL_BUILD=1
  )
ELSE()
  TARGET_COMPILE_DEFINITIONS(
    ${_target}
    PRIVATE HAVE_UNISTD_H=1 HAVE_DLFCN_H=1 USE_FLOAT
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

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
