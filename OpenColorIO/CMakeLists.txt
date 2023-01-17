#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "OpenColorIO"
)

SET(_sources
    AllocationOp.cpp
    FileFormatCCC.cpp
    GroupTransform.cpp
    NoOps.cpp
    AllocationTransform.cpp
    FileFormatCSP.cpp
    HashUtils.cpp
    OCIOYaml.cpp
    Baker.cpp
    FileFormatHDL.cpp
    ImageDesc.cpp
    Op.cpp
    CDLTransform.cpp
    FileFormatIridasCube.cpp
    ImagePacking.cpp
    OpOptimizers.cpp
    Caching.cpp
    FileFormatIridasItx.cpp
    LogOps.cpp
    ParseUtils.cpp
    ColorSpace.cpp
    FileFormatIridasLook.cpp
    LogTransform.cpp
    PathUtils.cpp
    ColorSpaceTransform.cpp
    FileFormatPandora.cpp
    Logging.cpp
    Processor.cpp
    Config.cpp
    FileFormatSpi1D.cpp
    Look.cpp
    ScanlineHelper.cpp
    Context.cpp
    FileFormatSpi3D.cpp
    LookParse.cpp
    Transform.cpp
    DisplayTransform.cpp
    FileFormatSpiMtx.cpp
    LookTransform.cpp
    TruelightOp.cpp
    Exception.cpp
    FileFormatTruelight.cpp
    Lut1DOp.cpp
    TruelightTransform.cpp
    ExponentOps.cpp
    FileFormatVF.cpp
    Lut3DOp.cpp
    UnitTest.cpp
    ExponentTransform.cpp
    FileTransform.cpp
    MathUtils.cpp
    Display.cpp
    FileFormat3DL.cpp
    GpuShaderDesc.cpp
    MatrixOps.cpp
    FileFormatCDL.cpp
    FileFormatCC.cpp
    GpuShaderUtils.cpp
    MatrixTransform.cpp
    Platform.cpp
    pystring/pystring.cpp
    md5/md5.cpp
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
  PUBLIC tinyxml
  PRIVATE yaml_cpp
)

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

IF(RV_TARGET_WINDOWS)
  TARGET_COMPILE_OPTIONS(
    ${_target}
    PRIVATE "-DOpenColorIO_EXPORTS=1" "-DOpenColorIO_SHARED=1"
  )
  TARGET_LINK_LIBRARIES(
    ${_target}
    PUBLIC Boost::headers Boost::system
  )
ENDIF()

RV_STAGE(TYPE "SHARED_LIBRARY" TARGET ${_target})
