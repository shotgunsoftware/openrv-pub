#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "yaml_cpp"
)

SET(_sources
    binary.cpp
    convert.cpp
    depthguard.cpp
    directives.cpp
    emit.cpp
    emitfromevents.cpp
    emitter.cpp
    emitterstate.cpp
    emitterutils.cpp
    exceptions.cpp
    exp.cpp
    memory.cpp
    node_data.cpp
    node.cpp
    nodebuilder.cpp
    nodeevents.cpp
    null.cpp
    ostream_wrapper.cpp
    parse.cpp
    parser.cpp
    regex_yaml.cpp
    scanner.cpp
    scanscalar.cpp
    scantag.cpp
    scantoken.cpp
    simplekey.cpp
    singledocparser.cpp
    stream.cpp
    tag.cpp
    contrib/graphbuilder.cpp
    contrib/graphbuilderadapter.cpp
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DYAML=TWKYAML"
)

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
