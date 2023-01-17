#
# Copyright (C) 2023  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "gc"
)

SET(_sources
    allchblk.c
    alloc.c
    blacklst.c
    checksums.c
    dbg_mlc.c
    dyn_load.c
    finalize.c
    gc_dlopen.c
    gcj_mlc.c
    headers.c
    malloc.c
    mallocx.c
    mark.c
    mark_rts.c
    misc.c
    new_hblk.c
    obj_map.c
    os_dep.c
    pcr_interface.c
    ptr_chck.c
    real_malloc.c
    reclaim.c
    specific.c
    stubborn.c
    typd_mlc.c
    backgraph.c
    thread_local_alloc.c
    mach_dep.c
    win32_threads.c
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gc
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PRIVATE atomic_ops::atomic_ops
)

ADD_COMPILE_DEFINITIONS(
  ALL_INTERIOR_POINTERS=1
  THREAD_LOCAL_ALLOC=1
  GC_THREADS=1
  GC_VERSION_MAJOR=7
  GC_VERSION_MINOR=2
  MAX_HEAP_SECTS=8192
  GC_BUILD
  GC_NOT_DLL
  _LIB
  GC_WIN32_THREADS
)

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
