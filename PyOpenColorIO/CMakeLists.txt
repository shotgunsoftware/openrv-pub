#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "PyOpenColorIO"
)

SET(_sources
    PyAllocationTransform.cpp
    PyConstants.cpp
    PyGroupTransform.cpp
    PyMatrixTransform.cpp
    PyCDLTransform.cpp
    PyContext.cpp
    PyLogTransform.cpp
    PyProcessor.cpp
    PyColorSpace.cpp
    PyDisplayTransform.cpp
    PyLook.cpp
    PyProcessorMetadata.cpp
    PyColorSpaceTransform.cpp
    PyExponentTransform.cpp
    PyLookTransform.cpp
    PyTransform.cpp
    PyConfig.cpp
    PyFileTransform.cpp
    PyMain.cpp
    PyUtil.cpp
    PyBaker.cpp
    PyGpuShaderDesc.cpp
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
  PUBLIC Python::Python OpenColorIO
)

TARGET_COMPILE_OPTIONS(
  ${_target}
  PRIVATE "-DHAVE_CONFIG_H" "-fno-strict-aliasing" "-DNDEBUG" "-fwrapv" "-Wall" "-DPYOCIO_NAME=PyOpenColorIO"
)

RV_STAGE(TYPE "PYTHON_PLUGIN" TARGET ${_target})
