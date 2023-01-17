# ##############################################################################################################################################################
# PYTHON UNIT TESTS ###

ADD_CUSTOM_TARGET(
  PYTests
  COMMAND ${PYTHON} OpenColorIOTestSuite.py ${CMAKE_BINARY_DIR}
  DEPENDS PyOpenColorIO
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Setting Up and Running PYTHON OCIO UNIT tests"
)

ADD_TEST(pyglue "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target PYTests)
