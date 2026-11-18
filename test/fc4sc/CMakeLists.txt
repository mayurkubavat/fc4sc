#******************************************************************************#
#   Copyright 2026 Mayur Kubavat
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#******************************************************************************#

# Use an installed googletest if there is one, and fetch it otherwise.
find_package(GTest 1.14 QUIET)

if(NOT TARGET GTest::gtest)
  include(FetchContent)

  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()

  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7)

  # Settings for the googletest build. These are plain variables, so that
  # they do not end up in the cache of a project which includes FC4SC.
  set(BUILD_GMOCK OFF)
  set(INSTALL_GTEST OFF)
  set(gtest_force_shared_crt ON)
  FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

file(GLOB FC4SC_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(fc4sc_tests ${FC4SC_TEST_SOURCES})
target_link_libraries(fc4sc_tests PRIVATE FC4SC::fc4sc GTest::gtest)

gtest_discover_tests(fc4sc_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# FC4SC_NO_THROW changes the inline code of the library, so the tests
# which depend on it are built into a separate executable.
add_executable(fc4sc_no_throw_tests
  main.cpp
  illegal_bin_sample_expression_test.cpp
  cross_bin_select_test.cpp)
target_compile_definitions(fc4sc_no_throw_tests PRIVATE FC4SC_NO_THROW)
target_link_libraries(fc4sc_no_throw_tests PRIVATE FC4SC::fc4sc GTest::gtest)

gtest_discover_tests(fc4sc_no_throw_tests
  TEST_PREFIX no_throw.
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
