#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#

INCLUDE(cxx_defaults)

SET(_target
    "nedmalloc"
)

SET(_sources
    nedmalloc.c
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DNEDMALLOC_BUILD=1"
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

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
