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
 * Tests for get_coverage of coverpoints and crosses when
 * type_option.merge_instances is set: their type coverage is computed on
 * the merged bins of all the instances.
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

class merged_cvp_cg : public covergroup {
public:

  CG_CONS(merged_cvp_cg) {};

  int SAMPLE_POINT(sample_x, cvp_x);
  int SAMPLE_POINT(sample_y, cvp_y);

  void sample(int x, int y) {
    sample_x = x;
    sample_y = y;
    covergroup::sample();
  }

  coverpoint<int> cvp_x = coverpoint<int> (this, "cvp_x",
    bin<int>("a", 1),
    bin<int>("b", 2),
    bin<int>("c", 3),
    bin<int>("d", 4)
  );

  coverpoint<int> cvp_y = coverpoint<int> (this, "cvp_y",
    bin<int>("p", 1),
    bin<int>("q", 2)
  );

  cross<int,int> crs = cross<int,int> (this, "crs", &cvp_x, &cvp_y);
};

} // namespace

TEST(merged_cvp_coverage, uses_merged_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  merged_cvp_cg cg1("cg1", __FILE__, __LINE__, cntxt);
  merged_cvp_cg cg2("cg2", __FILE__, __LINE__, cntxt);
  cg1.type_option().merge_instances = true;

  cg1.sample(1, 1);
  cg1.sample(2, 1);
  cg2.sample(2, 1);
  cg2.sample(3, 2);

  // coverpoint: bins a, b and c of 4 are hit in the merged instances
  int covered = -1, total = -1;
  EXPECT_EQ(cg1.cvp_x.get_coverage(covered, total), 75);
  EXPECT_EQ(covered, 3);
  EXPECT_EQ(total, 4);
  EXPECT_EQ(cg2.cvp_x.get_coverage(), 75);

  // cross: (a,p), (b,p) and (c,q) of 8
  EXPECT_EQ(cg1.crs.get_coverage(covered, total), 37.5);
  EXPECT_EQ(covered, 3);
  EXPECT_EQ(total, 8);

  // the sums agree with the merged coverage of the covergroup
  int cvg_covered = 0, cvg_total = 0;
  cg1.get_coverage(cvg_covered, cvg_total, cntxt);
  int sum_covered = 0, sum_total = 0;
  for (auto cvp : cg1.cvps) {
    cvp->get_coverage(covered, total);
    sum_covered += covered;
    sum_total += total;
  }
  EXPECT_EQ(sum_covered, cvg_covered);
  EXPECT_EQ(sum_total, cvg_total);
  EXPECT_DOUBLE_EQ(cg1.get_coverage(cntxt),
    (cg1.cvp_x.get_coverage() + cg1.cvp_y.get_coverage() + cg1.crs.get_coverage()) / 3);

  // without merge_instances, the average of the instances
  cg1.type_option().merge_instances = false;
  EXPECT_EQ(cg1.cvp_x.get_coverage(), 50);
  EXPECT_EQ(cg1.crs.get_coverage(), 25);

  fc4sc::global::delete_context(cntxt);
}

TEST(merged_cvp_coverage, options_apply_to_merged_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  merged_cvp_cg cg1("cg1", __FILE__, __LINE__, cntxt);
  merged_cvp_cg cg2("cg2", __FILE__, __LINE__, cntxt);
  cg1.type_option().merge_instances = true;

  cg1.sample(1, 1);
  cg2.sample(1, 1);

  // at_least applies to the merged hits
  cg1.cvp_x.option().at_least = 2;
  cg2.cvp_x.option().at_least = 2;
  EXPECT_EQ(cg1.cvp_x.get_coverage(), 25);
  EXPECT_EQ(cg1.cvp_x.get_inst_coverage(), 0);

  // the goal applies to the merged coverpoint
  cg1.cvp_x.option().goal = 25;
  EXPECT_EQ(cg1.cvp_x.get_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}
