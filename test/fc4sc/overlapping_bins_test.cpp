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

// A sampled value hits each bin which holds it once, whatever the order in
// which the bins and their intervals are given and however they overlap.
class overlapping_bins_cvg : public covergroup {
public:
  CG_CONS(overlapping_bins_cvg) {};

  int value = 0;

  void sample(int v) {
    this->value = v;
    covergroup::sample();
  }

  COVERPOINT(int, cvp, value) {
    // an interval inside another interval of the same bin
    bin<int>("inner", interval(12, 13), interval(10, 15)),
    // intervals with gaps between them
    bin<int>("gaps", interval(30, 31), interval(38, 44), interval(53, 56)),
    // a range which ends in the gap between two intervals of the map, and
    // one which ends after the last of them
    bin<int>("to_gap", interval(34, 50)),
    bin<int>("to_end", interval(40, 70)),
    // a bin which overlaps the others only at its ends
    bin<int>("ends", interval(15, 30), interval(70, 75))
  };
};

TEST(overlapping_bins, hit_counts) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    overlapping_bins_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    for (int v = 0; v < 80; ++v)
      cvg.sample(v);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(0), 6u);   // 10 to 15
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(1), 13u);  // 30, 31, 38 to 44, 53 to 56
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(2), 17u);  // 34 to 50
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(3), 31u);  // 40 to 70
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(4), 22u);  // 15 to 30, 70 to 75
    // 0 to 9, 32, 33 and 76 to 79 are in no bin
    EXPECT_EQ(cvg.cvp.get_misses(), 16u);
  }
  fc4sc::global::delete_context(cntxt);
}
