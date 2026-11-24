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

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool is_odd(int v) { return (v % 2) != 0; }
bool lt_12(int v) { return v < 12; }

class dist_first_cg : public covergroup {
public:
  CG_CONS(dist_first_cg, bool df = false) {
    type_option().distribute_first = df;
  };

  int x = 0;

  COVERPOINT(int, interval_cvp, x) {
    bin_array<int>("b", 4, interval(0, 15)).with(lt_12)
  };

  COVERPOINT(int, rem_cvp, x) {
    bin_array<int>("r", 3, interval(1, 5)).with(is_odd)
  };

  COVERPOINT(int, small_cvp, x) {
    bin_array<int>("s", 5, interval(1, 3)).with(is_odd)
  };

  COVERPOINT(int, vec_cvp, x) {
    bin_array<int>("v", 4, std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7})
      .with([](int v) { return v < 5; })
      .with(is_odd)
  };
};

class dist_first_override_cg : public covergroup {
public:
  CG_CONS(dist_first_override_cg) {
    type_option().distribute_first = true;
    cvp_override.type_option().distribute_first = false;
  };

  int x = 0;

  COVERPOINT(int, cvp_inherit, x) {
    bin_array<int>("b", 4, interval(0, 15)).with(lt_12)
  };

  COVERPOINT(int, cvp_override, x) {
    bin_array<int>("b", 4, interval(0, 15)).with(lt_12)
  };
};

class dist_first_cross_cg : public covergroup {
public:
  CG_CONS(dist_first_cross_cg) {
    type_option().distribute_first = true;
  };

  int a = 0;
  int b = 0;

  COVERPOINT(int, cvp_a, a) {
    bin_array<int>("ba", 3, interval(1, 5)).with(is_odd)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };

  cross<int, int> crs = cross<int, int>(
      this, "crs", &cvp_a, &cvp_b,
      cross_bin("ba_zero", binsof<int>(&cvp_a, "ba") && binsof<int>(&cvp_b, "zero")));
};

class dist_first_cross_idx_cg : public covergroup {
public:
  CG_CONS(dist_first_cross_idx_cg) {
    type_option().distribute_first = true;
  };

  int a = 0;
  int b = 0;

  // Under distribute_first=false: {8,9} -> ba[0], ba[1] (no ba[2]).
  // Under distribute_first=true: [0:3],[4:7],[8:11],[12:15] filtered -> only ba[2] survives.
  COVERPOINT(int, cvp_a, a) {
    bin_array<int>("ba", 4, interval(0, 15)).with([](int v) { return v >= 8 && v < 10; })
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };

  cross<int, int> crs = cross<int, int>(
      this, "crs", &cvp_a, &cvp_b,
      cross_bin("ba2_zero", binsof<int>(&cvp_a, "ba[2]") && binsof<int>(&cvp_b, "zero")));
};

} // namespace

TEST(distribute_first, false_vs_true_interval_and_remainder) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    dist_first_cg cg_false("cg_false", __FILE__, __LINE__, cntxt, false);
    EXPECT_FALSE(cg_false.type_option().distribute_first);
    EXPECT_FALSE(cg_false.interval_cvp.type_option().distribute_first);

    // Filter-first: [0..11] (12 vals) distributed into 4 bins of 3
    auto f_intv = cg_false.interval_cvp.get_bins_base();
    ASSERT_EQ(f_intv.size(), 4u);
    EXPECT_EQ(f_intv[0]->name(), "b[0]");
    EXPECT_EQ(f_intv[0]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{0, 2}}));
    EXPECT_EQ(f_intv[3]->name(), "b[3]");
    EXPECT_EQ(f_intv[3]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{9, 11}}));

    // Filter-first remainder: {1,3,5} into 3 bins -> r[0]={1}, r[1]={3}, r[2]={5}
    auto f_rem = cg_false.rem_cvp.get_bins_base();
    ASSERT_EQ(f_rem.size(), 3u);
    EXPECT_EQ(f_rem[0]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{1, 1}}));
    EXPECT_EQ(f_rem[1]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{3, 3}}));
    EXPECT_EQ(f_rem[2]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{5, 5}}));
  }
  fc4sc::global::delete_context(cntxt);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    dist_first_cg cg_true("cg_true", __FILE__, __LINE__, cntxt2, true);
    EXPECT_TRUE(cg_true.type_option().distribute_first);
    EXPECT_TRUE(cg_true.interval_cvp.type_option().distribute_first);

    // Distribute-first: [0..15] -> [0:3],[4:7],[8:11],[12:15], then filter <12 -> b[3] dropped
    auto t_intv = cg_true.interval_cvp.get_bins_base();
    ASSERT_EQ(t_intv.size(), 3u);
    EXPECT_EQ(t_intv[0]->name(), "b[0]");
    EXPECT_EQ(t_intv[0]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{0, 3}}));
    EXPECT_EQ(t_intv[1]->name(), "b[1]");
    EXPECT_EQ(t_intv[1]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{4, 7}}));
    EXPECT_EQ(t_intv[2]->name(), "b[2]");
    EXPECT_EQ(t_intv[2]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{8, 11}}));

    // Distribute-first remainder: [1:5] -> [1],[2],[3:5], then filter odd -> r[0]={1}, r[2]={3,5}
    auto t_rem = cg_true.rem_cvp.get_bins_base();
    ASSERT_EQ(t_rem.size(), 2u);
    EXPECT_EQ(t_rem[0]->name(), "r[0]");
    EXPECT_EQ(t_rem[0]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{1, 1}}));
    EXPECT_EQ(t_rem[1]->name(), "r[2]");
    EXPECT_EQ(t_rem[1]->get_intervals_to_int(),
              (std::vector<fc4sc::interval_t<int>>{{3, 3}, {5, 5}}));

    // Fewer values than bins: [1:3] across 5 -> s[0]={1}, s[1]={2}, s[2]={3}, filter odd -> s[0], s[2]
    auto t_small = cg_true.small_cvp.get_bins_base();
    ASSERT_EQ(t_small.size(), 2u);
    EXPECT_EQ(t_small[0]->name(), "s[0]");
    EXPECT_EQ(t_small[1]->name(), "s[2]");

    // Vector {0..7} across 4 -> {0,1},{2,3},{4,5},{6,7}, filter <5 && odd -> v[0]={1}, v[1]={3}
    auto t_vec = cg_true.vec_cvp.get_bins_base();
    ASSERT_EQ(t_vec.size(), 2u);
    EXPECT_EQ(t_vec[0]->name(), "v[0]");
    EXPECT_EQ(t_vec[0]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{1, 1}}));
    EXPECT_EQ(t_vec[1]->name(), "v[1]");
    EXPECT_EQ(t_vec[1]->get_intervals_to_int(), (std::vector<fc4sc::interval_t<int>>{{3, 3}}));

    cg_true.x = 5;
    cg_true.sample();
    EXPECT_EQ(cg_true.rem_cvp.get_bin_hit_count(1), 1u);

    xml_printer::coverage_save("distribute_first_test.xml", cntxt2);
  }
  fc4sc::global::delete_context(cntxt2);

  // UCIS.xsd forbids unknown attributes on <options>
  std::ifstream in("distribute_first_test.xml");
  std::stringstream ss;
  ss << in.rdbuf();
  EXPECT_EQ(ss.str().find("distribute_first="), std::string::npos);
}

TEST(distribute_first, per_coverpoint_override_cross_and_dynamic) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    dist_first_override_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.cvp_inherit.size(), 3u);
    EXPECT_EQ(cg.cvp_override.size(), 4u);

    dist_first_cross_cg ccg("ccg", __FILE__, __LINE__, cntxt);
    // cvp_a has 2 bins (r[0]={1}, r[2]={3,5}), cvp_b has 2 bins -> 4 products, 2 in ba_zero
    EXPECT_EQ(ccg.cvp_a.size(), 2u);
    EXPECT_EQ(ccg.crs.size(), 4u);
    ccg.a = 5;
    ccg.b = 0;
    ccg.sample();
    int covered = 0, total = 0;
    ccg.crs.get_inst_coverage(covered, total);
    EXPECT_EQ(covered, 1);
    EXPECT_EQ(total, 3); // ba_zero + 2 auto products

    dist_first_cross_idx_cg icg("icg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(icg.cvp_a.size(), 1u);
    EXPECT_EQ(icg.cvp_a.get_bins_base()[0]->name(), "ba[2]");
    EXPECT_EQ(icg.crs.size(), 2u);
    icg.a = 8;
    icg.b = 0;
    icg.sample();
    EXPECT_EQ(icg.crs.get_inst_coverage(covered, total), 50.0);
    EXPECT_EQ(covered, 1);
    EXPECT_EQ(total, 2);
  }
  fc4sc::global::delete_context(cntxt);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory fac("dyn_df_cg");
    int v = 0;
    auto df = fac.create_coverpoint<int()>("cvp", [&]() { return v; });
    df.create_bin_array("b", 4, interval(0, 15)); // regular bin_array
    fc4sc::dynamic_covergroup dcg(fac, "dcg", __FILE__, __LINE__, cntxt2);
    dcg.type_option().distribute_first = true;
    EXPECT_EQ(dcg.get_coverpoint("cvp").size(), 4u);
  }
  fc4sc::global::delete_context(cntxt2);
}
