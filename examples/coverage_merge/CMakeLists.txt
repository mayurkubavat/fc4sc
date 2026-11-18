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

# Merges UCIS XML files written by FC4SC. It does not need SystemC.
add_executable(fc4sc_merge fc4sc_merge.cpp)
target_link_libraries(fc4sc_merge PRIVATE FC4SC::fc4sc)

if(FC4SC_BUILD_TESTS)
  # the example writes two files, as two runs with different seeds, and
  # merges them with fc4sc_merge
  add_executable(coverage_merge_runs coverage_merge_runs.cpp)
  target_link_libraries(coverage_merge_runs PRIVATE FC4SC::fc4sc)
  add_test(NAME coverage_merge_runs COMMAND coverage_merge_runs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_test(NAME coverage_merge_tool
    COMMAND fc4sc_merge merged.xml run_1.xml run_2.xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(coverage_merge_tool PROPERTIES DEPENDS coverage_merge_runs)
endif()
