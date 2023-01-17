#
# Copyright (C) 2022  Autodesk, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

INCLUDE(cxx_defaults)

SET(_target
    "qtcolortriangle"
)

SET(_sources
    qtcolortriangle.cpp
)

FIND_PACKAGE(
  Qt5
  COMPONENTS Core
             Gui
             Network
             OpenGL
             Qml
             Quick
             QuickWidgets
             Svg
             Test
             UiTools
             WebChannel
             WebEngine
             WebEngineCore
             WebEngineWidgets
             Widgets
             Xml
             XmlPatterns
  REQUIRED
)

SET(CMAKE_AUTOUIC
    ON
)
SET(CMAKE_AUTOMOC
    ON
)
SET(CMAKE_AUTORCC
    ON
)

SET(_sources
    qtcolortriangle.cpp qtcolortriangle/qtcolortriangle.h # Triggers the AUTOMOC
)

ADD_LIBRARY(
  ${_target} STATIC
  ${_sources}
)

TARGET_INCLUDE_DIRECTORIES(
  ${_target}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_LINK_LIBRARIES(
  ${_target}
  PUBLIC Qt5::Gui Qt5::Widgets
  PRIVATE Qt5::Core
)

RV_STAGE(TYPE "LIBRARY" TARGET ${_target})
