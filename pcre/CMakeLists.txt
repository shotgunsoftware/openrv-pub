#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "pcre"
)

LIST(
  APPEND
  _sources
  pcre_chartables.c
  pcre_compile.c
  pcre_config.c
  pcre_dfa_exec.c
  pcre_exec.c
  pcre_fullinfo.c
  pcre_get.c
  pcre_globals.c
  pcre_info.c
  pcre_maketables.c
  pcre_newline.c
  pcre_ord2utf8.c
  pcre_refcount.c
  pcre_study.c
  pcre_tables.c
  pcre_try_flipped.c
  pcre_ucp_searchfuncs.c
  pcre_valid_utf8.c
  pcre_version.c
  pcre_xclass.c
  pcreposix.c
)

ADD_LIBRARY(
  ${_target} SHARED
  ${_sources}
)
TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
