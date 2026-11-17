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
 * Tests for get_inst_coverage(covered, total) of crosses, which must return
 * the same percentage as get_inst_coverage().
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

class cross_counts_cg : public covergroup {
public:

  CG_CONS(cross_counts_cg) {};

  int SAMPLE_POINT(sample_a, cvp_a);
  int SAMPLE_POINT(sample_b, cvp_b);

  void sample(int a, int b) {
    sample_a = a;
    sample_b = b;
    covergroup::sample();
  }

  coverpoint<int> cvp_a = coverpoint<int> (this, "cvp_a",
    bin<int>("a0", 0),
    bin<int>("a1", 1)
  );

  coverpoint<int> cvp_b = coverpoint<int> (this, "cvp_b",
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  );

  cross<int,int> crs = cross<int,int> (this, "crs", &cvp_a, &cvp_b);
};

} // namespace

TEST(cross_get_inst_coverage, counts_overload_returns_percentage) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_counts_cg cg("cg", __FILE__, __LINE__, cntxt);

  int covered = -1, total = -1;
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), 0);
  EXPECT_EQ(covered, 0);
  EXPECT_EQ(total, 4);

  cg.sample(0, 0);
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), 25);
  EXPECT_EQ(covered, 1);
  EXPECT_EQ(total, 4);
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), cg.crs.get_inst_coverage());

  // the goal applies as in the overload without counts
  cg.crs.option().goal = 25;
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), 100);
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), cg.crs.get_inst_coverage());
  cg.crs.option().goal = 100;

  cg.sample(0, 1);
  cg.sample(1, 0);
  cg.sample(1, 1);
  EXPECT_EQ(cg.crs.get_inst_coverage(covered, total), 100);
  EXPECT_EQ(covered, 4);

  fc4sc::global::delete_context(cntxt);
}

TEST(cross_get_inst_coverage, covergroup_counts_overload_agrees) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_counts_cg cg("cg", __FILE__, __LINE__, cntxt);

  cg.sample(0, 0);
  cg.sample(1, 1);

  // coverpoints at 100%, cross at 50%
  int covered = -1, total = -1;
  EXPECT_DOUBLE_EQ(cg.get_inst_coverage(), 250.0 / 3);
  EXPECT_DOUBLE_EQ(cg.get_inst_coverage(covered, total), cg.get_inst_coverage());
  EXPECT_EQ(covered, 6);
  EXPECT_EQ(total, 8);

  fc4sc::global::delete_context(cntxt);
}
