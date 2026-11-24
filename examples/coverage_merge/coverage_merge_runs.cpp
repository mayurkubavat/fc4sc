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
 * Runs a covergroup with two seeds and writes the coverage of each run to
 * run_<seed>.xml, for fc4sc_merge. The second run also starts from the
 * coverage of the first, as with $load_coverage_db, and writes the total
 * to cumulative.xml.
 */

#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

#include "fc4sc.hpp"

class packet_cvg : public covergroup {
public:
  CG_CONS(packet_cvg) {};

  int length = 0;
  int kind = 0;

  COVERPOINT(int, length_cvp, length) {
    bin<int>("short", interval(0, 15)),
    bin_array<int>("medium", 4, interval(16, 63)),
    bin<int>("long", interval(64, 255))
  };

  COVERPOINT(int, kind_cvp, kind) {
    bin<int>("read", 0),
    bin<int>("write", 1),
    bin<int>("atomic", 2)
  };

  cross<int, int> length_x_kind = cross<int, int>(this, "length_x_kind", &length_cvp, &kind_cvp);
};

static double run(unsigned seed, int samples, bool load_previous)
{
  auto cntxt = fc4sc::global::create_new_context();
  double coverage = 0;
  {
    packet_cvg cvg("packets", __FILE__, __LINE__, cntxt);
    if (load_previous)
      fc4sc::load_coverage_db("run_1.xml", cntxt);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> length(0, 255), kind(0, 2);
    for (int i = 0; i < samples; ++i) {
      cvg.length = length(gen);
      cvg.kind = kind(gen);
      cvg.sample();
    }
    fc4sc::global::set_coverage_db_name(load_previous ? "cumulative.xml" : "run_" + std::to_string(seed) + ".xml", cntxt);
    xml_printer::coverage_save("", cntxt);
    coverage = cvg.get_inst_coverage();
  }
  fc4sc::global::delete_context(cntxt);
  return coverage;
}

int main()
{
  std::cout << "run 1: " << run(1, 10, false) << "%\n";
  std::cout << "run 2: " << run(2, 10, false) << "%\n";
  std::cout << "run 2 after run 1: " << run(2, 10, true) << "%\n";
  return 0;
}
