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

#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

enum class query_color_e : uint8_t {
  RED = 0,
  GREEN = 2,
  BLUE = 5
};

} // namespace

FC4SC_ENUM_VALUES(query_color_e,
                  query_color_e::RED,
                  query_color_e::GREEN,
                  query_color_e::BLUE)

namespace {

class query_int_cg : public covergroup {
public:
  CG_CONS(query_int_cg) {};

  int a = 0;
  int b = 0;
  double d = 0.0;
  uint8_t w = 0;
  query_color_e col = query_color_e::RED;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("b0", interval(0, 3)),
    bin<int>("b1", interval(4, 7)),
    bin<int>("b2", interval(10, 12)),
    ignore_bin<int>("ign", interval(2, 5)),
    illegal_bin<int>("ill", 11),
    default_bin<int>("def")
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("low", 0),
    bin<int>("mid", 1),
    bin<int>("high", 2),
    transition_bin<int>("tr", trans<int>(0, 1))
  };

  COVERPOINT(double, cvp_d, d) {
    bin<double>("tol", tolerance(10.0, 1.5)),
    bin<double>("ptol", percent_tolerance(100.0, 10.0)),
    ignore_bin<double>("d_ign", interval(9.5, 10.5))
  };

  COVERPOINT(uint8_t, cvp_wc, w) {
    bin<uint8_t>("wc", wildcard<uint8_t>("8'b0000_01??")),
    bin_array<uint8_t>("arr", 2, interval<uint8_t>(10, 13))
  };

  COVERPOINT(query_color_e, cvp_col, col) {};
};

class query_auto_cg : public covergroup {
public:
  CG_CONS(query_auto_cg) {
    option().auto_bin_max = 4;
  };

  uint8_t x = 0;
  COVERPOINT(uint8_t, cvp_x, x) {};
};

class query_bounds_cg : public covergroup {
public:
  CG_CONS(query_bounds_cg) {};

  uint8_t u = 0;
  COVERPOINT(uint8_t, cvp_u, u) {
    bin<uint8_t>("all", interval<uint8_t>(0, 255)),
    ignore_bin<uint8_t>("ign_lo", uint8_t(0)),
    illegal_bin<uint8_t>("ill_hi", uint8_t(255)),
    ignore_bin<uint8_t>("ign_mid", interval<uint8_t>(100, 150))
  };
};

class query_cross_cg : public covergroup {
public:
  CG_CONS(query_cross_cg) {};

  int a = 0;
  int b = 0;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("a0", interval(0, 1)),
    bin<int>("a1", interval(2, 3)),
    ignore_bin<int>("a_ign", 1)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 10),
    bin<int>("b1", 20),
    illegal_bin<int>("b_ill", 99)
  };

  cross<int, int> crs_auto = cross<int, int>(this, "crs_auto", &cvp_a, &cvp_b);

  cross<int, int> crs_user = cross<int, int>(
      this, "crs_user", &cvp_a, &cvp_b,
      cross_bin("u_a0", binsof<int>(&cvp_a, "a0")),
      cross_bin("u_empty", binsof<int>(&cvp_a, "a0") && binsof<int>(&cvp_a, "a1")),
      ignore_cross_bin("ign_b1", binsof<int>(&cvp_b, "b1") && binsof<int>(&cvp_a, "a1")),
      default_cross_bin("rest"));

  cross<int, int> crs_with = cross<int, int>(
      this, "crs_with", &cvp_a, &cvp_b,
      cross_with([](int va, int vb) { return (va + vb) != 12; }),
      illegal_cross_bin("ill_0_20", CrossQueueType<int, int>{{0, 20}}));
};

class query_double_queue_cross_cg : public covergroup {
public:
  CG_CONS(query_double_queue_cross_cg) {};

  double d = 0.0;
  int k = 0;

  COVERPOINT(double, cvp_d, d) {
    bin<double>("d0", interval(0.0, 5.0)),
    bin<double>("d1", interval(10.0, 15.0))
  };

  COVERPOINT(int, cvp_k, k) {
    bin<int>("k0", interval(0, 1))
  };

  cross<double, int> crs_dq = cross<double, int>(
      this, "crs_dq", &cvp_d, &cvp_k,
      cross_bin("q_pts", CrossQueueType<double, int>{
          {1.25, 0},
          {12.5, 1},
          {12.5, 0}
      }),
      ignore_cross_bin("q_ign", CrossQueueType<double, int>{{12.5, 0}}),
      default_cross_bin("rest"));
};

class query_strobe_cg : public covergroup {
public:
  CG_CONS(query_strobe_cg, bool enable_strobe = false) {
    type_option().strobe = enable_strobe;
  };

  int v = 0;
  COVERPOINT(int, cvp_v, v) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };
};

} // namespace

TEST(uncovered_query, bin_and_coverpoint_intervals_const) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_int_cg cg("cg", __FILE__, __LINE__, cntxt);
    const query_int_cg& ccg = cg;
    const coverpoint<int>& c_cvp_a = cg.cvp_a;

    // Initially: b0=[0,3]-ign[2,5]=[0,1], b1=[4,7]-ign[2,5]=[6,7], b2=[10,12]-ill[11]=[10,10],[12,12].
    auto ivs = c_cvp_a.get_uncovered_intervals();
    ASSERT_EQ(ivs.size(), 4u);
    EXPECT_EQ(ivs[0], (fc4sc::interval<int>(0, 1)));
    EXPECT_EQ(ivs[1], (fc4sc::interval<int>(6, 7)));
    EXPECT_EQ(ivs[2], (fc4sc::interval<int>(10, 10)));
    EXPECT_EQ(ivs[3], (fc4sc::interval<int>(12, 12)));

    // Per-bin query on b0
    auto b0_ivs = c_cvp_a.get_uncovered_intervals(0);
    ASSERT_EQ(b0_ivs.size(), 1u);
    EXPECT_EQ(b0_ivs[0], (fc4sc::interval<int>(0, 1)));

    // Direct bin<T>::get_uncovered_intervals
    auto bins_base = c_cvp_a.get_bins_base();
    ASSERT_GE(bins_base.size(), 3u);
    const auto* typed_b0 = dynamic_cast<const bin<int>*>(bins_base[0]);
    ASSERT_NE(typed_b0, nullptr);
    auto raw_b0_ivs = typed_b0->get_uncovered_intervals(1);
    ASSERT_EQ(raw_b0_ivs.size(), 1u);
    EXPECT_EQ(raw_b0_ivs[0], (fc4sc::interval<int>(0, 3)));

    // Hit b0 with 0 -> b0 covered; b1, b2 remain
    cg.a = 0;
    cg.d = 11.0; // hits tol [8.5, 11.5]
    cg.w = 5;    // hits wc [4, 7]
    cg.col = query_color_e::GREEN;
    cg.sample();

    auto after_a = ccg.get_uncovered_intervals(cg.cvp_a);
    ASSERT_EQ(after_a.size(), 3u);
    EXPECT_EQ(after_a[0], (fc4sc::interval<int>(6, 7)));

    // Double tolerance & percent_tolerance subtraction
    auto d_ivs = ccg.get_uncovered_intervals<double>("cvp_d");
    // tol was hit, so only ptol [90.0, 110.0] remains (outside d_ign [9.5, 10.5])
    ASSERT_EQ(d_ivs.size(), 1u);
    EXPECT_DOUBLE_EQ(d_ivs[0].first, 90.0);
    EXPECT_DOUBLE_EQ(d_ivs[0].second, 110.0);

    // Wildcard & bin_array: wc hit, arr[0]=[10,11] and arr[1]=[12,13] merge to [10,13]!
    auto wc_ivs = ccg.get_uncovered_intervals<uint8_t>("cvp_wc");
    ASSERT_EQ(wc_ivs.size(), 1u);
    EXPECT_EQ(wc_ivs[0].first, static_cast<uint8_t>(10));
    EXPECT_EQ(wc_ivs[0].second, static_cast<uint8_t>(13));

    // Enum class auto-bins: RED and BLUE remain uncovered
    auto col_ivs = ccg.get_uncovered_intervals(cg.cvp_col);
    ASSERT_EQ(col_ivs.size(), 2u);
    EXPECT_EQ(col_ivs[0], (fc4sc::interval<query_color_e>(query_color_e::RED, query_color_e::RED)));
    EXPECT_EQ(col_ivs[1], (fc4sc::interval<query_color_e>(query_color_e::BLUE, query_color_e::BLUE)));

    // Transition bin skipped by get_uncovered_intervals(), included in get_uncovered_bins()
    // Note: cg.b was 0 during the earlier cg.sample(), so low(0) is covered; mid(1) and high(2) remain -> [1,2]
    auto tr_ivs = ccg.get_uncovered_intervals(cg.cvp_b);
    ASSERT_EQ(tr_ivs.size(), 1u);
    EXPECT_EQ(tr_ivs[0], (fc4sc::interval<int>(1, 2)));

    // Untyped coverpoint_base and 0-arg covergroup map
    const fc4sc::coverpoint_base& c_base = cg.cvp_a;
    auto str_ivs = c_base.get_uncovered_intervals_str();
    ASSERT_EQ(str_ivs.size(), 3u);
    EXPECT_EQ(str_ivs[0].first, "6");
    EXPECT_EQ(str_ivs[0].second, "7");
    auto int_ivs = c_base.get_uncovered_intervals_to_int();
    ASSERT_EQ(int_ivs.size(), 3u);
    EXPECT_EQ(int_ivs[0], (fc4sc::interval<int>(6, 7)));

    auto cg_map = ccg.get_uncovered_intervals();
    EXPECT_EQ(cg_map.size(), 5u);
    EXPECT_EQ(cg_map.at("cvp_wc").size(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(uncovered_query, discrete_boundary_and_at_least) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_bounds_cg cg("cg_bounds", __FILE__, __LINE__, cntxt);
    auto ivs = cg.cvp_u.get_uncovered_intervals();
    ASSERT_EQ(ivs.size(), 2u);
    EXPECT_EQ(ivs[0].first, static_cast<uint8_t>(1));
    EXPECT_EQ(ivs[0].second, static_cast<uint8_t>(99));
    EXPECT_EQ(ivs[1].first, static_cast<uint8_t>(151));
    EXPECT_EQ(ivs[1].second, static_cast<uint8_t>(254));

    cg.cvp_u.option().at_least = 2;
    cg.u = 10;
    cg.sample();
    // Hits == 1 < at_least(2), still uncovered
    EXPECT_EQ(cg.cvp_u.get_uncovered_intervals().size(), 2u);
    EXPECT_EQ(cg.cvp_u.get_uncovered_bins().size(), 1u);

    cg.sample();
    // Hits == 2 >= at_least(2), now covered
    EXPECT_TRUE(cg.cvp_u.get_uncovered_intervals().empty());
    EXPECT_TRUE(cg.cvp_u.get_uncovered_bins().empty());

    // at_least == 0: nothing is uncovered
    cg.cvp_u.option().at_least = 0;
    EXPECT_TRUE(cg.cvp_u.get_uncovered_intervals().empty());
    EXPECT_TRUE(cg.cvp_u.get_uncovered_bins().empty());

    query_auto_cg acg("acg", __FILE__, __LINE__, cntxt);
    // uint8_t split into 4 auto bins: [0:63], [64:127], [128:191], [192:255]
    EXPECT_EQ(acg.cvp_x.get_uncovered_bins().size(), 4u);
    auto aivs = acg.cvp_x.get_uncovered_intervals();
    ASSERT_EQ(aivs.size(), 1u);
    EXPECT_EQ(aivs[0].first, static_cast<uint8_t>(0));
    EXPECT_EQ(aivs[0].second, static_cast<uint8_t>(255));
    acg.x = 70;
    acg.sample();
    EXPECT_EQ(acg.cvp_x.get_uncovered_bins().size(), 3u);
    auto aivs2 = acg.cvp_x.get_uncovered_intervals();
    ASSERT_EQ(aivs2.size(), 2u);
    EXPECT_EQ(aivs2[0], (fc4sc::interval<uint8_t>(0, 63)));
    EXPECT_EQ(aivs2[1], (fc4sc::interval<uint8_t>(128, 255)));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(uncovered_query, uncovered_bins_and_infos_coverpoint_and_cross) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_int_cg icg("icg", __FILE__, __LINE__, cntxt);
    const auto infos = icg.cvp_b.get_uncovered_bin_infos();
    ASSERT_EQ(infos.size(), 4u);
    EXPECT_EQ(infos[0].name, "low");
    EXPECT_FALSE(infos[0].is_transition);
    EXPECT_FALSE(infos[0].is_cross);
    EXPECT_EQ(infos[3].name, "tr");
    EXPECT_TRUE(infos[3].is_transition);

    icg.b = 0;
    icg.sample();
    icg.b = 1;
    icg.sample();
    // low, mid, tr are now covered; only high remains
    auto rem_bins = icg.cvp_b.get_uncovered_bins();
    ASSERT_EQ(rem_bins.size(), 1u);
    EXPECT_EQ(rem_bins[0], "high");

    query_cross_cg ccg("ccg", __FILE__, __LINE__, cntxt);
    const query_cross_cg& cccg = ccg;

    // crs_auto: 2x2 = 4 auto bins: <a0,b0>, <a0,b1>, <a1,b0>, <a1,b1>
    auto auto_bins = cccg.crs_auto.get_uncovered_bins();
    ASSERT_EQ(auto_bins.size(), 4u);
    EXPECT_EQ(auto_bins[0], "<a0,b0>");
    EXPECT_EQ(auto_bins[1], "<a0,b1>");
    EXPECT_EQ(auto_bins[2], "<a1,b0>");
    EXPECT_EQ(auto_bins[3], "<a1,b1>");

    // crs_user: u_a0 (size=2, regular), u_empty (size=0 -> excluded!),
    // ign_b1 (ignore -> excluded!), rest (default -> excluded!).
    auto user_bins = cccg.crs_user.get_uncovered_bins();
    ASSERT_EQ(user_bins.size(), 1u);
    EXPECT_EQ(user_bins[0], "u_a0");

    // Sample (0, 10) -> covers <a0,b0> in crs_auto and u_a0 in crs_user
    ccg.a = 0;
    ccg.b = 10;
    ccg.sample();
    EXPECT_EQ(cccg.crs_auto.get_uncovered_bins().size(), 3u);
    EXPECT_TRUE(cccg.crs_user.get_uncovered_bins().empty());

    // at_least > 1 on cross
    ccg.crs_auto.option().at_least = 2;
    EXPECT_EQ(cccg.crs_auto.get_uncovered_bins().size(), 4u);
    ccg.crs_auto.option().at_least = 0;
    EXPECT_TRUE(cccg.crs_auto.get_uncovered_bins().empty());
    ccg.crs_auto.option().at_least = 1;

    // Covergroup-level forwarders
    auto cg_bins_map = cccg.get_uncovered_bins();
    EXPECT_EQ(cg_bins_map.at("crs_auto").size(), 3u);
    EXPECT_EQ(cccg.get_uncovered_bins("crs_user").size(), 0u);
    auto cg_infos_map = cccg.get_uncovered_bin_infos();
    EXPECT_EQ(cg_infos_map.at("crs_auto").size(), 3u);
    EXPECT_TRUE(cg_infos_map.at("crs_auto")[0].is_cross);
    EXPECT_TRUE(cg_infos_map.at("crs_auto")[0].is_auto);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(uncovered_query, uncovered_cross_tuples_with_queue_ignore_illegal_truncation) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_cross_cg ccg("ccg", __FILE__, __LINE__, cntxt);
    const query_cross_cg& cccg = ccg;

    // cvp_a valid values: {0, 2, 3} (1 is ignore_bin!)
    // cvp_b valid values: {10, 20} (99 is illegal_bin!)
    // crs_with has cross_with(a + b != 12) -> excludes (2, 10)!
    // and illegal_cross_bin {(0, 20)} -> excludes (0, 20)!
    // So feasible tuples in crs_with are:
    // (0, 10), (2, 20), (3, 10), (3, 20).
    bool truncated = true;
    auto tuples = cccg.crs_with.get_uncovered_cross_tuples(64, &truncated);
    EXPECT_FALSE(truncated);
    ASSERT_EQ(tuples.size(), 4u);
    EXPECT_EQ(tuples[0], (std::make_tuple(0, 10)));
    EXPECT_EQ(tuples[1], (std::make_tuple(2, 20)));
    EXPECT_EQ(tuples[2], (std::make_tuple(3, 10)));
    EXPECT_EQ(tuples[3], (std::make_tuple(3, 20)));

    // Truncation bound test
    auto bounded = cccg.get_uncovered_cross_tuples<int, int>("crs_with", 2, &truncated);
    EXPECT_TRUE(truncated);
    EXPECT_EQ(bounded.size(), 2u);

    // Sample (3, 10) -> hits bin <a1, b0>, leaving <a0, b0>={(0,10)} and <a1, b1>={(2,20),(3,20)}
    ccg.a = 3;
    ccg.b = 10;
    ccg.sample();
    auto after_hit = cccg.get_uncovered_cross_tuples(ccg.crs_with, 64, &truncated);
    EXPECT_FALSE(truncated);
    ASSERT_EQ(after_hit.size(), 3u);
    EXPECT_EQ(after_hit[0], (std::make_tuple(0, 10)));
    EXPECT_EQ(after_hit[1], (std::make_tuple(2, 20)));
    EXPECT_EQ(after_hit[2], (std::make_tuple(3, 20)));

    // crs_user: only u_a0 is covered-counted; valid tuples in a0={0,1}-ignore{1}={0}, b={10,20}:
    // (0, 10), (0, 20)
    auto user_tuples = cccg.crs_user.get_uncovered_cross_tuples(64, &truncated);
    EXPECT_FALSE(truncated);
    ASSERT_EQ(user_tuples.size(), 2u);
    EXPECT_EQ(user_tuples[0], (std::make_tuple(0, 10)));
    EXPECT_EQ(user_tuples[1], (std::make_tuple(0, 20)));

    // Stringified covergroup-level cross tuples map
    auto str_map = cccg.get_uncovered_cross_tuples(64, &truncated);
    EXPECT_EQ(str_map.at("crs_user").size(), 2u);
    EXPECT_EQ(str_map.at("crs_user")[0], (std::vector<std::string>{"0", "10"}));
  }
  fc4sc::global::delete_context(cntxt);

  // Queue-backed cross on non-enumerable double coverpoint!
  auto cntxt2 = fc4sc::global::create_new_context();
  {
    query_double_queue_cross_cg dqcg("dqcg", __FILE__, __LINE__, cntxt2);
    bool truncated = false;
    auto q_tuples = dqcg.crs_dq.get_uncovered_cross_tuples(64, &truncated);
    // (12.5, 0) is excluded by ignore_cross_bin; (1.25, 0) and (12.5, 1) remain!
    EXPECT_FALSE(truncated);
    ASSERT_EQ(q_tuples.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<0>(q_tuples[0]), 1.25);
    EXPECT_EQ(std::get<1>(q_tuples[0]), 0);
    EXPECT_DOUBLE_EQ(std::get<0>(q_tuples[1]), 12.5);
    EXPECT_EQ(std::get<1>(q_tuples[1]), 1);
  }
  fc4sc::global::delete_context(cntxt2);
}

TEST(strobe_option, default_false_ucis_omitted_and_no_backend) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_strobe_cg cg_def("cg_def", __FILE__, __LINE__, cntxt, false);
    EXPECT_FALSE(cg_def.type_option().strobe);
    cg_def.v = 0;
    cg_def.sample();
    EXPECT_EQ(cg_def.cvp_v.get_bin_hit_count(0), 1u);

    // Setting strobe=true without SystemC strobe backend
    cg_def.type_option().strobe = true;
    EXPECT_TRUE(cg_def.type_option().strobe);
    cg_def.v = 1;
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(cg_def.sample());
#else
    EXPECT_THROW(cg_def.sample(), std::logic_error);
#endif

    // Stop still calls on_stopped_sample() without throwing
    cg_def.stop();
    EXPECT_NO_THROW(cg_def.sample());
    cg_def.start();

    xml_printer::coverage_save("strobe_option_ucis.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream in("strobe_option_ucis.xml");
  ASSERT_TRUE(in.good());
  std::stringstream ss;
  ss << in.rdbuf();
  EXPECT_EQ(ss.str().find("strobe="), std::string::npos);
}

namespace {

class query_not_with_and_iff_cg : public covergroup {
public:
  CG_CONS(query_not_with_and_iff_cg) {};

  int a = 0;
  int b = 0;
  bool bin_en = false;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("a0", interval(0, 1)),
    bin<int>("a1", interval(2, 3))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 10),
    bin<int>("b1", 20)
  };

  // !binsof(...).with(v == 0) selects values {1, 2, 3} x {10, 20} = 6 tuples;
  // uncovered auto bins add {(0, 10), (0, 20)} = 2 tuples -> 8 total.
  cross<int, int> crs_not_with = cross<int, int>(
      this, "crs_not_with", &cvp_a, &cvp_b,
      cross_bin("not_zero", !binsof<int>(&cvp_a).with([](int v) { return v == 0; })));

  // cross_bin.iff(false) must still appear in both get_uncovered_bins() and get_uncovered_cross_tuples()
  cross<int, int> crs_bin_iff = cross<int, int>(
      this, "crs_bin_iff", &cvp_a, &cvp_b,
      cross_bin("u0", binsof<int>(&cvp_a, "a0")).iff(&bin_en),
      default_cross_bin("def"));

  // Covered user bin with iff(false) must not leak into auto bins
  cross<int, int> crs_covered_bin_iff = cross<int, int>(
      this, "crs_covered_bin_iff", &cvp_a, &cvp_b,
      cross_bin("u0", binsof<int>(&cvp_a, "a0")).iff(&bin_en));
};

} // namespace

TEST(uncovered_query, negated_with_cross_bin_iff_and_zero_budget_truncation) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    query_not_with_and_iff_cg cg("cg", __FILE__, __LINE__, cntxt);
    const query_not_with_and_iff_cg& ccg = cg;

    // 1. !binsof(...).with(...) must not drop uncovered tuples in pre-filter
    bool truncated = true;
    auto not_with_tuples = ccg.crs_not_with.get_uncovered_cross_tuples(64, &truncated);
    EXPECT_FALSE(truncated);
    EXPECT_EQ(ccg.crs_not_with.get_uncovered_bins().size(), 3u); // not_zero, <a0,b0>, <a0,b1>
    ASSERT_EQ(not_with_tuples.size(), 8u);
    EXPECT_EQ(not_with_tuples[0], (std::make_tuple(0, 10)));
    EXPECT_EQ(not_with_tuples[1], (std::make_tuple(0, 20)));
    EXPECT_EQ(not_with_tuples[2], (std::make_tuple(1, 10)));
    EXPECT_EQ(not_with_tuples[7], (std::make_tuple(3, 20)));

    // 2. cross_bin.iff(false) consistency between get_uncovered_bins() and get_uncovered_cross_tuples()
    cg.bin_en = false;
    auto iff_bins = ccg.crs_bin_iff.get_uncovered_bins();
    auto iff_tuples = ccg.crs_bin_iff.get_uncovered_cross_tuples(64, &truncated);
    EXPECT_FALSE(truncated);
    ASSERT_EQ(iff_bins.size(), 1u);
    EXPECT_EQ(iff_bins[0], "u0");
    ASSERT_EQ(iff_tuples.size(), 4u);
    EXPECT_EQ(iff_tuples[0], (std::make_tuple(0, 10)));
    EXPECT_EQ(iff_tuples[3], (std::make_tuple(1, 20)));

    // Covered user bin with iff(false) must not leak into auto bins
    cg.bin_en = true;
    cg.a = 0; cg.b = 10; cg.sample(); // covers crs_covered_bin_iff.u0
    cg.bin_en = false;
    auto cov_iff_tuples = ccg.crs_covered_bin_iff.get_uncovered_cross_tuples(64, &truncated);
    // Only <a1, b0> and <a1, b1> auto bins remain (4 tuples), not u0's (0,10)/(0,20)/(1,10)/(1,20)
    ASSERT_EQ(cov_iff_tuples.size(), 4u);
    EXPECT_EQ(cov_iff_tuples[0], (std::make_tuple(2, 10)));

    // 3. max_tuples == 0: truncated is true when uncovered tuples exist, false when 100% covered
    bool trunc_zero_uncov = false;
    auto zero_uncov = ccg.crs_covered_bin_iff.get_uncovered_cross_tuples(0, &trunc_zero_uncov);
    EXPECT_TRUE(zero_uncov.empty());
    EXPECT_TRUE(trunc_zero_uncov);

    cg.a = 2; cg.b = 10; cg.sample();
    cg.a = 2; cg.b = 20; cg.sample();
    ASSERT_TRUE(ccg.crs_covered_bin_iff.get_uncovered_bins().empty());

    bool trunc_zero_cov = true;
    auto zero_cov = ccg.crs_covered_bin_iff.get_uncovered_cross_tuples(0, &trunc_zero_cov);
    EXPECT_TRUE(zero_cov.empty());
    EXPECT_FALSE(trunc_zero_cov);
  }
  fc4sc::global::delete_context(cntxt);
}
