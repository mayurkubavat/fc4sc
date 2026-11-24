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
  cross_bin_select_test.cpp
  cross_with_queue_default_iff_test.cpp
  uncovered_query_test.cpp)
target_compile_definitions(fc4sc_no_throw_tests PRIVATE FC4SC_NO_THROW)
target_link_libraries(fc4sc_no_throw_tests PRIVATE FC4SC::fc4sc GTest::gtest)

gtest_discover_tests(fc4sc_no_throw_tests
  TEST_PREFIX no_throw.
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

find_package(SystemCLanguage CONFIG QUIET)
if(TARGET SystemC::systemc)
  add_executable(fc4sc_systemc_tests
    systemc/systemc_types_test.cpp
    systemc/systemc_types_order2_test.cpp
    systemc/systemc_strobe_test.cpp
    systemc/systemc_toggle_test.cpp
    systemc/systemc_sva_test.cpp)
  target_link_libraries(fc4sc_systemc_tests PRIVATE FC4SC::fc4sc SystemC::systemc GTest::gtest)
  gtest_discover_tests(fc4sc_systemc_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  find_package(crave CONFIG QUIET)
  if(TARGET crave::crave)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES crave::crave)
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_compiles("
      #include <crave/ConstrainedRandom.hpp>
      #include <crave/experimental/Experimental.hpp>
      int main() {
        crave::crv_variable<int> x;
        auto s = crave::soft(x() == 1);
        (void)s;
        return 0;
      }
    " FC4SC_CRAVE_HAS_SOFT)
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_QUIET)

    add_executable(fc4sc_crave_tests
      crave/crave_cdv_test.cpp)
    target_link_libraries(fc4sc_crave_tests PRIVATE FC4SC::fc4sc crave::crave SystemC::systemc GTest::gtest)
    if(FC4SC_CRAVE_HAS_SOFT)
      target_compile_definitions(fc4sc_crave_tests PRIVATE FC4SC_CRAVE_HAS_SOFT=1)
    endif()
    gtest_discover_tests(fc4sc_crave_tests
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  find_package(UVMSystemC CONFIG QUIET)
  find_package(crave2uvm CONFIG QUIET)
  if(TARGET crave2uvm::crave2uvm AND TARGET UVMSystemC::uvm-systemc)
    add_executable(fc4sc_crave2uvm_tests
      crave/crave2uvm_cdv_test.cpp)
    target_link_libraries(fc4sc_crave2uvm_tests PRIVATE FC4SC::fc4sc crave2uvm::crave2uvm crave::crave UVMSystemC::uvm-systemc GTest::gtest)
    gtest_discover_tests(fc4sc_crave2uvm_tests
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()
