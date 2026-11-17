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
 * Tests for get_coverage of coverpoints and crosses: their type coverage,
 * across all the instances of the covergroup type they are in.
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

class options_methods_cg : public covergroup {
public:

  CG_CONS(options_methods_cg) {};

  int SAMPLE_POINT(sample_a, cvp_a);
  int SAMPLE_POINT(sample_b, cvp_b);

  void sample(int a, int b) {
    sample_a = a;
    sample_b = b;
    covergroup::sample();
  }

  coverpoint<int> cvp_a = coverpoint<int> (this, "cvp_a",
    bin<int>("a0", 0),
    bin<int>("a1", 1),
    bin<int>("a2", 2),
    bin<int>("a3", 3)
  );

  coverpoint<int> cvp_b = coverpoint<int> (this, "cvp_b",
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  );

  cross<int,int> crs = cross<int,int> (this, "crs", &cvp_a, &cvp_b);
};

class overlap_cg : public covergroup {
public:

  CG_CONS(overlap_cg) {};

  int SAMPLE_POINT(sample_v, cvp);

  void sample(int v) {
    sample_v = v;
    covergroup::sample();
  }

  coverpoint<int> cvp = coverpoint<int> (this, "cvp",
    bin<int>("low", interval(0, 5)),
    bin<int>("high", interval(3, 8)),
    bin<int>("apart", interval(10, 12)),
    illegal_bin<int>("bad", 4)
  );
};

} // namespace

TEST(cvp_get_coverage, single_instance_equals_inst_coverage) {
  auto cntxt = fc4sc::global::create_new_context();
  options_methods_cg cg("cg", __FILE__, __LINE__, cntxt);

  int covered = -1, total = -1;
  EXPECT_EQ(cg.cvp_a.get_coverage(), 0);
  EXPECT_EQ(cg.cvp_a.get_coverage(covered, total), 0);
  EXPECT_EQ(covered, 0);
  EXPECT_EQ(total, 4);

  cg.sample(0, 0);
  cg.sample(1, 1);
  EXPECT_EQ(cg.cvp_a.get_coverage(), cg.cvp_a.get_inst_coverage());
  EXPECT_EQ(cg.cvp_a.get_coverage(covered, total), 50);
  EXPECT_EQ(covered, 2);
  EXPECT_EQ(total, 4);

  EXPECT_EQ(cg.crs.get_coverage(), cg.crs.get_inst_coverage());
  EXPECT_EQ(cg.crs.get_coverage(covered, total), 25);
  EXPECT_EQ(covered, 2);
  EXPECT_EQ(total, 8);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvp_get_coverage, across_instances) {
  auto cntxt = fc4sc::global::create_new_context();
  options_methods_cg cg1("cg1", __FILE__, __LINE__, cntxt);
  options_methods_cg cg2("cg2", __FILE__, __LINE__, cntxt);

  cg1.sample(0, 0);
  cg1.sample(1, 0);
  cg2.sample(0, 0);

  // coverpoint: 50% in cg1, 25% in cg2
  int covered = -1, total = -1;
  EXPECT_EQ(cg1.cvp_a.get_inst_coverage(), 50);
  EXPECT_EQ(cg2.cvp_a.get_inst_coverage(), 25);
  EXPECT_EQ(cg1.cvp_a.get_coverage(), 37.5);
  EXPECT_EQ(cg2.cvp_a.get_coverage(covered, total), 37.5);
  EXPECT_EQ(covered, 3);
  EXPECT_EQ(total, 8);

  // cross: 2 of 8 bins in cg1, 1 of 8 in cg2
  EXPECT_EQ(cg1.crs.get_coverage(covered, total), (25 + 12.5) / 2);
  EXPECT_EQ(covered, 3);
  EXPECT_EQ(total, 16);

  // the instances are weighted with the covergroup instance weights
  cg2.option().weight = 3;
  EXPECT_EQ(cg1.cvp_a.get_coverage(), (50 + 3 * 25) / 4.0);
  cg2.option().weight = 0;
  EXPECT_EQ(cg1.cvp_a.get_coverage(), 50);
  cg1.option().weight = 0;
  EXPECT_EQ(cg1.cvp_a.get_coverage(), 100);
  cg1.option().weight = 1;
  cg2.option().weight = 1;

  // the goal of the coverpoint applies in each instance
  cg1.cvp_a.option().goal = 50;
  EXPECT_EQ(cg1.cvp_a.get_coverage(), (100 + 25) / 2.0);
  cg1.cvp_a.option().goal = 100;

  // through the base class, as get_coverpoint returns it
  EXPECT_EQ(cg2.get_coverpoint("cvp_a").get_coverage(), 37.5);
  EXPECT_EQ(cg2.get_coverpoint("crs").get_coverage(), (25 + 12.5) / 2);

  // instances of other covergroup types do not count
  overlap_cg other("other", __FILE__, __LINE__, cntxt);
  other.sample(0);
  EXPECT_EQ(cg1.cvp_a.get_coverage(), 37.5);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvp_get_coverage, covergroup_type_coverage_agrees) {
  auto cntxt = fc4sc::global::create_new_context();
  options_methods_cg cg1("cg1", __FILE__, __LINE__, cntxt);
  options_methods_cg cg2("cg2", __FILE__, __LINE__, cntxt);

  cg1.sample(0, 0);
  cg1.sample(3, 1);
  cg2.sample(2, 1);

  // with the default options the type coverage of the covergroup is the
  // average of the type coverages of its coverpoints and crosses
  double avg = (cg1.cvp_a.get_coverage() + cg1.cvp_b.get_coverage() + cg1.crs.get_coverage()) / 3;
  EXPECT_DOUBLE_EQ(cg1.get_coverage(cntxt), avg);

  int cvg_covered = 0, cvg_total = 0;
  cg1.get_coverage(cvg_covered, cvg_total, cntxt);
  int sum_covered = 0, sum_total = 0;
  for (auto cvp : cg1.cvps) {
    int covered = 0, total = 0;
    cvp->get_coverage(covered, total);
    sum_covered += covered;
    sum_total += total;
  }
  EXPECT_EQ(sum_covered, cvg_covered);
  EXPECT_EQ(sum_total, cvg_total);

  fc4sc::global::delete_context(cntxt);
}
