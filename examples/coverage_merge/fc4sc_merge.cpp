/******************************************************************************

   Copyright 2026 Mayur Kubavat

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

******************************************************************************/

/*
 * Merges UCIS XML files written by FC4SC into one:
 *
 *   fc4sc_merge merged.xml run_1.xml run_2.xml ...
 */

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"

int main(int argc, char* argv[])
{
  if (argc < 3) {
    std::cerr << "usage: " << argv[0] << " <merged.xml> <input.xml>...\n";
    return 2;
  }
  std::vector<std::string> inputs(argv + 2, argv + argc);
  try {
    fc4sc::merge_coverage_db(inputs, argv[1]);
  }
  catch (const std::exception& e) {
    std::cerr << argv[0] << ": " << e.what() << "\n";
    return 1;
  }
  std::cout << "Merged " << inputs.size() << " files into " << argv[1] << "\n";
  return 0;
}
