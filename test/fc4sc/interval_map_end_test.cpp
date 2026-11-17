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
#include "fc4sc.hpp"
#include "gtest/gtest.h"

// Each bin added after the bins of the same kind whose values are all
// lower than its own appends its values to the end of the flat map of the
// coverpoint. The map was read at its end() iterator when this happened,
// which AddressSanitizer reports as an overflow.
class interval_map_end_cvg : public covergroup {
public:
  CG_CONS(interval_map_end_cvg) {};

  int value = 0;

  void sample(int v) {
    this->value = v;
    covergroup::sample();
  }

  COVERPOINT(int, cvp, value) {
    bin<int>("one", 1),
    ignore_bin<int>("ignore_eight", 8),
    bin<int>("low", interval(2, 3)),
    ignore_bin<int>("ignore_ten", 10),
    bin<int>("four", 4),
    ignore_bin<int>("ignore_twelve", interval(12, 20)),
    bin<int>("high", interval(30, 40))
  };
};

TEST(interval_map, add_at_end) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    interval_map_end_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    for (int v : {1, 2, 3, 4, 8, 10, 15, 35, 50})
      cvg.sample(v);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(1), 2u);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(3), 1u);
    // 8, 10, 15 are ignored and 50 is in no bin
    EXPECT_EQ(cvg.cvp.get_misses(), 4u);
  }
  fc4sc::global::delete_context(cntxt);
}
