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

cmake_minimum_required(VERSION 3.14)

project(FC4SC LANGUAGES CXX)

# Installing and testing are only enabled by default when FC4SC is the
# top-level project, and not when it is added with add_subdirectory().
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(FC4SC_IS_TOP_LEVEL ON)
else()
  set(FC4SC_IS_TOP_LEVEL OFF)
endif()

option(FC4SC_INSTALL "Install the FC4SC headers and CMake package" ${FC4SC_IS_TOP_LEVEL})
option(FC4SC_BUILD_TESTS "Build the FC4SC unit tests (requires googletest)" OFF)
option(FC4SC_BUILD_EXAMPLES "Build the FC4SC examples (requires SystemC)" OFF)

#------------------------------------------------------------------------------#
# Header-only library target
#------------------------------------------------------------------------------#

include(GNUInstallDirs)

add_library(fc4sc INTERFACE)
add_library(FC4SC::fc4sc ALIAS fc4sc)

target_include_directories(fc4sc INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fc4sc>)
target_compile_features(fc4sc INTERFACE cxx_std_11)

#------------------------------------------------------------------------------#
# Installation and CMake package
#------------------------------------------------------------------------------#

if(FC4SC_INSTALL)
  include(CMakePackageConfigHelpers)

  set(FC4SC_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FC4SC)

  install(DIRECTORY includes/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fc4sc
    FILES_MATCHING PATTERN "*.hpp")

  install(TARGETS fc4sc EXPORT FC4SCTargets)

  install(EXPORT FC4SCTargets
    NAMESPACE FC4SC::
    DESTINATION ${FC4SC_INSTALL_CMAKEDIR})

  configure_package_config_file(
    cmake/FC4SCConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FC4SCConfig.cmake
    INSTALL_DESTINATION ${FC4SC_INSTALL_CMAKEDIR})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FC4SCConfig.cmake
    DESTINATION ${FC4SC_INSTALL_CMAKEDIR})
endif()

#------------------------------------------------------------------------------#
# Unit tests and examples
#------------------------------------------------------------------------------#

if(FC4SC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test/fc4sc)
endif()

if(FC4SC_BUILD_EXAMPLES)
  add_subdirectory(examples/fir)
  add_subdirectory(examples/coverage_merge)
endif()
