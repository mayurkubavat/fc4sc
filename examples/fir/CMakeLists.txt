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

find_package(SystemCLanguage CONFIG REQUIRED)

file(GLOB FC4SC_FIR_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(fir ${FC4SC_FIR_SOURCES})
target_compile_definitions(fir PRIVATE FC4SC_NO_THROW)
target_link_libraries(fir PRIVATE FC4SC::fc4sc SystemC::systemc)

if(FC4SC_BUILD_TESTS)
  # The example samples illegal bins on purpose, so check that the
  # simulation ran to its end instead of relying on the exit code.
  add_test(NAME fir_example COMMAND fir
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(fir_example PROPERTIES
    PASS_REGULAR_EXPRESSION "Simulation of 24 items finished")
endif()
