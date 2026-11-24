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
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

enum class color_e { red = 0, green = 1, blue = 2 };

} // namespace

namespace fc4sc {
template <>
struct fc4sc_enum_traits<color_e> {
  static constexpr bool is_specialized = true;
  static std::vector<color_e> values() {
    return {color_e::red, color_e::green, color_e::blue};
  }
  static std::vector<std::pair<color_e, std::string>> named_values() {
    return {{color_e::red, "red"}, {color_e::green, "green"}, {color_e::blue, "blue"}};
  }
};
} // namespace fc4sc

namespace {

static const fc4sc::cross_bin_data_model& find_user_bin(
    fc4sc::cross_base& crs, const std::string& name) {
  (void)crs.size();
  auto* data = static_cast<fc4sc::cross_base_data_model*>(crs.get_data());
  for (const auto& b : data->user_bins) {
    if (b.name == name) return b;
  }
  throw std::invalid_argument("missing cross bin: " + name);
}

static CrossQueueType<int, int> make_diag_queue() {
  CrossQueueType<int, int> q;
  for (int i = 0; i < 4; ++i) q.push_back(CrossValType<int, int>(i, i));
  return q;
}

class cross_with_cvg : public covergroup {
public:
  CG_CONS(cross_with_cvg) {};

  int a = 0;
  int b = 0;
  int c = 0;
  bool gate = true;

  void sample(int a_val, int b_val, int c_val = 0) {
    a = a_val;
    b = b_val;
    c = c_val;
    covergroup::sample();
  }

  COVERPOINT(int, cvp_a, a) {
    bin<int>("low", interval(0, 1)),
    bin<int>("high", interval(2, 3))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("low", interval(0, 1)),
    bin<int>("high", interval(2, 3))
  };

  COVERPOINT(int, cvp_c, c) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };

  // User cross bins with binsof.with, cross_bin.with, ignore.with, illegal.with
  cross<int, int> axb_bins_with = cross<int, int>(
      this, "axb_bins_with", &cvp_a, &cvp_b,
      cross_bin("diag", binsof<int>(&cvp_a) && binsof<int>(&cvp_b))
          .with([](int va, int vb) { return va == vb; }),
      cross_bin("even_a", binsof<int>(&cvp_a).with([](int va) { return va % 2 == 0; }) &&
                              binsof<int>(&cvp_b, "high")),
      ignore_cross_bin("ign_sum3").with([](int va, int vb) { return va + vb == 3; }),
      illegal_cross_bin("bad_gt5").with([](const std::tuple<int, int>& t) {
        return std::get<0>(t) + std::get<1>(t) > 5;
      }));

  // Cross-level .with() chained as rvalue (tests C++11 move constructor)
  cross<int, int> axb_cross_with =
      cross<int, int>(this, "axb_cross_with", &cvp_a, &cvp_b)
          .with([](int va, int vb) { return va <= vb; });

  // 3-way cross with select_expression::with and cross_with()
  cross<int, int, int> abc_with = cross<int, int, int>(
      this, "abc_with", &cvp_a, &cvp_b, &cvp_c,
      cross_bin("sum_eq",
                (binsof<int>(&cvp_a) && binsof<int>(&cvp_b) && binsof<int>(&cvp_c))
                    .with<int, int, int>([](int va, int vb, int vc) { return va + vb == vc; })),
      cross_with([](int va, int vb, int vc) { return vc == 0 || va == vb; }));
};

class cross_non_enum_with_cvg : public covergroup {
public:
  CG_CONS(cross_non_enum_with_cvg) {};
  double d = 0.0;
  int k = 0;

  void sample(double dv, int kv) {
    d = dv;
    k = kv;
    covergroup::sample();
  }

  COVERPOINT(double, cvp_d, d) {
    bin<double>("unit", interval(0.0, 1.0)),
    bin<double>("two", 2.0)
  };

  COVERPOINT(int, cvp_k, k) {}; // M1 auto_bin_max=64 over 32-bit int

  cross<double, int> dxk = cross<double, int>(
      this, "dxk", &cvp_d, &cvp_k,
      cross_bin("half_pos").with([](double dv, int kv) { return dv > 0.5 && kv > 0; }));
};

class cross_queue_default_iff_cvg : public covergroup {
public:
  CG_CONS(cross_queue_default_iff_cvg) {};

  int a = 0;
  int b = 0;
  bool allow_crs = true;
  bool allow_bin = true;

  void sample(int va, int vb) {
    a = va;
    b = vb;
    covergroup::sample();
  }

  COVERPOINT(int, cvp_a, a) {
    bin<int>("b0", 0),
    bin<int>("b1", 1),
    bin<int>("b2", 2),
    bin<int>("b3", 3),
    ignore_bin<int>("ign", 9)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 0),
    bin<int>("b1", 1),
    bin<int>("b2", 2),
    bin<int>("b3", 3)
  };

  using diag_queue_t = cross<int, int>::CrossQueueType;

  cross<int, int> q_crs = cross<int, int>(
      this, "q_crs", &cvp_a, &cvp_b,
      cross_bin("diag", make_diag_queue),
      cross_bin("anti", diag_queue_t{{0, 3}, {1, 2}}).iff([this]() { return allow_bin; }),
      ignore_cross_bin("ign_q", CrossQueueType<int, int>{{2, 3}, {9, 0}, {99, 99}}),
      illegal_cross_bin("bad_q", CrossQueueType<int, int>{{3, 0}}),
      default_cross_bin("rest"),
      CROSS_IFF(allow_crs));

  cross<int, int> arr_crs = cross<int, int>(
      this, "arr_crs", &cvp_a, &cvp_b,
      cross_bin_array("pts", CrossQueueType<int, int>{{0, 0}, {1, 1}, {99, 0}}));
};

class cross_enum_double_queue_cvg : public covergroup {
public:
  CG_CONS(cross_enum_double_queue_cvg) {};
  color_e col = color_e::red;
  double val = 0.0;

  void sample(color_e c, double v) {
    col = c;
    val = v;
    covergroup::sample();
  }

  COVERPOINT(color_e, cvp_col, col) {};
  COVERPOINT(double, cvp_val, val) {
    bin<double>("zero", 0.0),
    bin<double>("mid", interval(0.5, 2.5))
  };

  cross<color_e, double> c_x_v = cross<color_e, double>(
      this, "c_x_v", &cvp_col, &cvp_val,
      cross_bin("sel", CrossQueueType<color_e, double>{
          {color_e::red, 0.0}, {color_e::green, 1.5}}),
      default_cross_bin("other"));
};

class cross_remediation_cg : public covergroup {
public:
  CG_CONS(cross_remediation_cg) {};
  int a = 0;
  int b = 0;
  bool c1 = false;
  bool c2 = false;
  SAMPLE_CG(a, b);

  COVERPOINT(int, cvp_a, a) {
    bin<int>("r", interval(0, 5))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("r", interval(0, 5))
  };

  // Bug 1: binsof.with(pred).intersect(...) and chained .with().intersect().with()
  cross<int, int> crs_with_intersect = cross<int, int>(
      this, "crs_with_intersect", &cvp_a, &cvp_b,
      cross_bin("wi", binsof<int>(&cvp_a).with([](int x) { return x > 2; }).intersect(interval(0, 5)) &&
                      binsof<int>(&cvp_b).intersect(interval(0, 5)).with([](int y) { return y < 3; })),
      cross_bin("wiw", binsof<int>(&cvp_a).with([](int x) { return x >= 1; })
                                          .intersect(interval(0, 5))
                                          .with([](int x) { return x <= 3; }) &&
                       binsof<int>(&cvp_b, "r")),
      default_cross_bin("rest"));

  // Bug 2: pure CrossQueueType + cross_bin_array shadowed by ignore_cross_bin
  cross<int, int> crs_q_ignored = cross<int, int>(
      this, "crs_q_ignored", &cvp_a, &cvp_b,
      cross_bin("q_full_ign", CrossQueueType<int, int>{{0, 0}, {1, 1}}),
      cross_bin("q_part_ign", CrossQueueType<int, int>{{0, 0}, {2, 2}}),
      ignore_cross_bin("ign", CrossQueueType<int, int>{{0, 0}, {1, 1}}));

  cross<int, int> crs_arr_ignored = cross<int, int>(
      this, "crs_arr_ignored", &cvp_a, &cvp_b,
      cross_bin_array("qa", CrossQueueType<int, int>{{0, 0}, {3, 3}}),
      ignore_cross_bin("ign0", CrossQueueType<int, int>{{0, 0}}),
      default_cross_bin("rest"));

  // Minor 6b: chained cross_bin::iff(c1).iff(c2) composes with &&
  cross<int, int> crs_chained_iff = cross<int, int>(
      this, "crs_chained_iff", &cvp_a, &cvp_b,
      cross_bin("pair", CrossQueueType<int, int>{{1, 1}})
          .iff([this]() { return c1; })
          .iff(&c2),
      default_cross_bin("rest"));
};

class cross_q_illegal_cg : public covergroup {
public:
  CG_CONS(cross_q_illegal_cg) {};
  int a = 0;
  int b = 0;
  SAMPLE_CG(a, b);

  COVERPOINT(int, cvp_a, a) {
    bin<int>("r", interval(0, 5))
  };
  COVERPOINT(int, cvp_b, b) {
    bin<int>("r", interval(0, 5))
  };

  cross<int, int> crs_q_illegal = cross<int, int>(
      this, "crs_q_illegal", &cvp_a, &cvp_b,
      cross_bin("q_ill", CrossQueueType<int, int>{{0, 0}, {1, 1}}),
      illegal_cross_bin("ill", binsof<int>(&cvp_a)),
      default_cross_bin("rest"));
};

class cross_non_enum_queue_with_cg : public covergroup {
public:
  CG_CONS(cross_non_enum_queue_with_cg) {};
  int a = 0;
  double d = 0.0;
  SAMPLE_CG(a, d);

  COVERPOINT(int, cvp_wide, a) {}; // M1 auto_bin_max=64 over 32-bit int
  COVERPOINT(double, cvp_dbl, d) {
    bin<double>("zero_to_ten", interval(0.0, 10.0))
  };

  // Bug 3: CrossQueueType(...).with(pred) on non-enumerable coverpoints
  cross<int, double> crs_non_enum_q = cross<int, double>(
      this, "crs_non_enum_q", &cvp_wide, &cvp_dbl,
      cross_bin("q_impossible", CrossQueueType<int, double>{{0, 1.0}})
          .with([](int x, double y) { return x > 100 && y > 0.0; }),
      cross_bin("q_partial", CrossQueueType<int, double>{{0, 1.0}, {5, 2.0}})
          .with([](int x, double y) { return x > 2 && y > 1.5; }),
      default_cross_bin("rest"));
};

} // namespace

TEST(cross_with, tuple_and_element_predicates) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto& x = cvg.axb_bins_with;

    // 4 total bin products: (low,low), (low,high), (high,low), (high,high)
    // diag covers tuples (0,0),(1,1) in (low,low) and (2,2),(3,3) in (high,high)
    // ign_sum3 covers (0,3),(1,2) in (low,high) and (2,1),(3,0) in (high,low)
    // bad_gt5 covers (3,3) in (high,high)
    EXPECT_EQ(find_user_bin(x, "diag").size, 2u);
    EXPECT_EQ(find_user_bin(x, "even_a").size, 2u);
    EXPECT_EQ(find_user_bin(x, "ign_sum3").size, 2u);
    EXPECT_EQ(find_user_bin(x, "bad_gt5").size, 1u);

    // Sample (0, 0): hits diag
    cvg.sample(0, 0, 0);
    EXPECT_EQ(find_user_bin(x, "diag").hits, 1u);
    EXPECT_EQ(find_user_bin(x, "even_a").hits, 0u);

    // Sample (0, 1): in (low, low) product, fails diag (0 != 1), hits auto-bin (0, 0)
    cvg.sample(0, 1, 0);
    EXPECT_EQ(find_user_bin(x, "diag").hits, 1u);
    EXPECT_EQ(x.get_cross_bins().size(), 1u);

    // Sample (0, 3): matches ign_sum3 -> ignored (neither even_a nor auto-bin hits)
    cvg.sample(0, 3, 0);
    EXPECT_EQ(find_user_bin(x, "ign_sum3").hits, 1u);
    EXPECT_EQ(find_user_bin(x, "even_a").hits, 0u);

    // Sample (0, 2): matches even_a
    cvg.sample(0, 2, 0);
    EXPECT_EQ(find_user_bin(x, "even_a").hits, 1u);

    // Sample (3, 3): illegal bad_gt5
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(cvg.sample(3, 3, 0));
#else
    EXPECT_THROW(cvg.sample(3, 3, 0), fc4sc::illegal_bin_sample_exception);
#endif
    EXPECT_EQ(find_user_bin(x, "bad_gt5").hits, 1u);

    // Cross-level .with (va <= vb): (0,0), (0,1), (0,3), (0,2), (3,3) all pass;
    // (2, 0) fails va <= vb -> increments misses, no auto bin added
    const size_t before_bins = cvg.axb_cross_with.get_cross_bins().size();
    const uint64_t before_misses = cvg.axb_cross_with.get_misses();
    cvg.sample(2, 0, 0);
    EXPECT_EQ(cvg.axb_cross_with.get_cross_bins().size(), before_bins);
    EXPECT_EQ(cvg.axb_cross_with.get_misses(), before_misses + 1);

    // 3-way cross: (0, 1, 1) fails cross_with (vc == 0 || va == vb) -> must not hit
    const uint64_t sum_eq_before = find_user_bin(cvg.abc_with, "sum_eq").hits;
    cvg.sample(0, 1, 1);
    EXPECT_EQ(find_user_bin(cvg.abc_with, "sum_eq").hits, sum_eq_before);
    // (0, 0, 0) passes cross_with and sum_eq
    cvg.sample(0, 0, 0);
    EXPECT_EQ(find_user_bin(cvg.abc_with, "sum_eq").hits, sum_eq_before + 1);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_with, non_enumerable_double_and_wide_auto_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_non_enum_with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // Must construct and query coverage immediately without hanging or OOM
    int covered = 0, total = 0;
    EXPECT_EQ(cvg.dxk.get_inst_coverage(covered, total), 0.0);
    EXPECT_GT(total, 0);

    cvg.sample(0.75, 10);
    EXPECT_EQ(find_user_bin(cvg.dxk, "half_pos").hits, 1u);
    // Product (0, pos_bin) is Partial, so sampling (0.25, 10) hits the auto-bin
    cvg.sample(0.25, 10);
    EXPECT_FALSE(cvg.dxk.get_cross_bins().empty());
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_with, stopped_coverpoint_does_not_leak_stale_value) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(0, 0, 0);
    EXPECT_EQ(find_user_bin(cvg.axb_bins_with, "diag").hits, 1u);

    cvg.cvp_a.stop();
    cvg.sample(0, 0, 0);
    // Must not re-hit diag using stale cvp_a hit/value
    EXPECT_EQ(find_user_bin(cvg.axb_bins_with, "diag").hits, 1u);
    EXPECT_EQ(cvg.axb_bins_with.get_misses(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_queue_default_iff, queue_default_iff_and_xml_db_roundtrip) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_queue_default_iff_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto& q = cvg.q_crs;
    auto* q_data = static_cast<fc4sc::cross_base_data_model*>(q.get_data());

    EXPECT_EQ(q_data->auto_bins_size, 0u);
    EXPECT_EQ(find_user_bin(q, "diag").size, 4u);
    EXPECT_EQ(find_user_bin(q, "anti").size, 2u);
    EXPECT_EQ(find_user_bin(q, "ign_q").size, 1u); // {9,0} & {99,99} are out of range
    EXPECT_EQ(find_user_bin(q, "bad_q").size, 1u);
    EXPECT_GT(find_user_bin(q, "rest").size, 0u);

    // cross_bin_array creates pts[0], pts[1]; {99, 0} out of range -> size 0
    EXPECT_EQ(find_user_bin(cvg.arr_crs, "pts[0]").size, 1u);
    EXPECT_EQ(find_user_bin(cvg.arr_crs, "pts[1]").size, 1u);
    EXPECT_EQ(find_user_bin(cvg.arr_crs, "pts[2]").size, 0u);

    // Cross coverage only counts regular user bins (diag, anti) when default_cross_bin exists
    int covered = 0, total = 0;
    EXPECT_EQ(q.get_inst_coverage(covered, total), 0.0);
    EXPECT_EQ(total, 2);

    // Hit diag (0,0), (1,1)
    cvg.sample(0, 0);
    cvg.sample(1, 1);
    EXPECT_EQ(find_user_bin(q, "diag").hits, 2u);
    EXPECT_EQ(q.get_inst_coverage(covered, total), 50.0);

    // Sample (2, 1) -> hits default_cross_bin "rest", does not increase coverage
    cvg.sample(2, 1);
    EXPECT_EQ(find_user_bin(q, "rest").hits, 1u);
    EXPECT_EQ(q.get_inst_coverage(covered, total), 50.0);

    // Sample (2, 3) -> ignored, must NOT hit "rest"
    cvg.sample(2, 3);
    EXPECT_EQ(find_user_bin(q, "ign_q").hits, 1u);
    EXPECT_EQ(find_user_bin(q, "rest").hits, 1u);

    // Disable per-bin iff on "anti", sample (0, 3) -> falls through to "rest"
    cvg.allow_bin = false;
    cvg.sample(0, 3);
    EXPECT_EQ(find_user_bin(q, "anti").hits, 0u);
    EXPECT_EQ(find_user_bin(q, "rest").hits, 2u);

    // Re-enable anti iff and hit (1, 2) -> coverage becomes 100%
    cvg.allow_bin = true;
    cvg.sample(1, 2);
    EXPECT_EQ(find_user_bin(q, "anti").hits, 1u);
    EXPECT_EQ(q.get_inst_coverage(covered, total), 100.0);

    // Cross iff == false: illegal (3, 0) must NOT throw or increment misses or hit rest
    cvg.allow_crs = false;
    const uint64_t misses_before = q.get_misses();
    EXPECT_NO_THROW(cvg.sample(3, 0));
    EXPECT_EQ(q.get_misses(), misses_before);
    EXPECT_EQ(find_user_bin(q, "bad_q").hits, 0u);
    EXPECT_EQ(find_user_bin(q, "rest").hits, 2u);

    // Cross iff == true: illegal (3, 0) throws when !FC4SC_NO_THROW
    cvg.allow_crs = true;
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(cvg.sample(3, 0));
#else
    EXPECT_THROW(cvg.sample(3, 0), fc4sc::illegal_bin_sample_exception);
#endif
    EXPECT_EQ(find_user_bin(q, "bad_q").hits, 1u);

    xml_printer::coverage_save("cross_with_queue_default_iff_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  // Verify XML structure & default_cross_bin type="ignore"
  std::ifstream ifs("cross_with_queue_default_iff_test.xml");
  std::stringstream buf;
  buf << ifs.rdbuf();
  const std::string xml = buf.str();
  EXPECT_NE(xml.find("name=\"rest\""), std::string::npos);
  EXPECT_NE(xml.find("type=\"ignore\""), std::string::npos);

  // Reload into a fresh context & merge round-trip
  auto cntxt2 = fc4sc::global::create_new_context();
  {
    cross_queue_default_iff_cvg cvg2("cvg", __FILE__, __LINE__, cntxt2);
    uint64_t loaded = fc4sc::load_coverage_db("cross_with_queue_default_iff_test.xml", cntxt2);
    EXPECT_GT(loaded, 0u);
    EXPECT_EQ(find_user_bin(cvg2.q_crs, "diag").hits, 2u);
    EXPECT_EQ(find_user_bin(cvg2.q_crs, "anti").hits, 1u);
    EXPECT_EQ(find_user_bin(cvg2.q_crs, "rest").hits, 2u);
    EXPECT_EQ(cvg2.q_crs.get_inst_coverage(), 100.0);

    // Merge two XMLs and verify default_cross_bin doesn't collide with regular bin
    fc4sc::merge_coverage_db(
        {"cross_with_queue_default_iff_test.xml", "cross_with_queue_default_iff_test.xml"},
        "cross_with_queue_default_iff_merged.xml");
  }
  fc4sc::global::delete_context(cntxt2);

  auto cntxt3 = fc4sc::global::create_new_context();
  {
    cross_queue_default_iff_cvg cvg_m("cvg", __FILE__, __LINE__, cntxt3);
    fc4sc::load_coverage_db("cross_with_queue_default_iff_merged.xml", cntxt3);
    EXPECT_EQ(find_user_bin(cvg_m.q_crs, "diag").hits, 4u);
    EXPECT_EQ(find_user_bin(cvg_m.q_crs, "anti").hits, 2u);
    EXPECT_EQ(find_user_bin(cvg_m.q_crs, "rest").hits, 4u);
  }
  fc4sc::global::delete_context(cntxt3);
}

TEST(cross_queue_type, enum_class_double_and_empty_queue) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_enum_double_queue_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(find_user_bin(cvg.c_x_v, "sel").size, 2u);
    cvg.sample(color_e::red, 0.0);
    cvg.sample(color_e::green, 1.5);
    cvg.sample(color_e::blue, 1.0);
    EXPECT_EQ(find_user_bin(cvg.c_x_v, "sel").hits, 2u);
    EXPECT_EQ(find_user_bin(cvg.c_x_v, "other").hits, 1u);
    EXPECT_EQ(cvg.c_x_v.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_m2_dynamic, create_instance_preserves_with_queue_default_iff) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory fac("dyn_cross_m2", __FILE__, __LINE__);
    int a = 0, b = 0;
    bool en = true;
    auto cp_a = fac.create_coverpoint<int()>("a", [&]() { return a; });
    cp_a.create_bin("all", interval(0, 3));
    auto cp_b = fac.create_coverpoint<int()>("b", [&]() { return b; });
    cp_b.create_bin("all", interval(0, 3));

    auto* raw_a = cp_a.get_cvp_fact();
    auto* raw_b = cp_b.get_cvp_fact();
    fac.add_cross_with_bins<int, int>(
        "axb", raw_a, raw_b,
        cross_bin("diag", CrossQueueType<int, int>{{0, 0}, {1, 1}}),
        default_cross_bin("other"),
        cross_iff([&]() { return en; }));

    fc4sc::dynamic_covergroup inst(fac, "inst", __FILE__, __LINE__, cntxt);
    cp_a.bind_sample(inst);
    cp_b.bind_sample(inst);
    a = 0; b = 0;
    inst.sample();
    en = false;
    a = 2; b = 3;
    inst.sample(); // cross_iff false -> other must stay 0
    auto& crs = static_cast<fc4sc::cross_base&>(inst.get_coverpoint("axb"));
    EXPECT_EQ(find_user_bin(crs, "diag").hits, 1u);
    EXPECT_EQ(find_user_bin(crs, "other").hits, 0u);
    en = true;
    inst.sample();
    EXPECT_EQ(find_user_bin(crs, "other").hits, 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_remediation, binsof_with_intersect_queue_exclusions_and_chained_iff) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_remediation_cg cg("cg", __FILE__, __LINE__, cntxt);

    // Bug 1: binsof.with(pred).intersect(...) preserves pred (and chained .with().intersect().with())
    cg.sample(1, 1); // x=1 fails x>2 in wi, passes 1<=x<=3 in wiw
    EXPECT_EQ(find_user_bin(cg.crs_with_intersect, "wi").hits, 0u);
    EXPECT_EQ(find_user_bin(cg.crs_with_intersect, "wiw").hits, 1u);

    cg.sample(4, 1); // x=4, y=1 passes wi (x>2 && y<3), fails wiw (x<=3)
    EXPECT_EQ(find_user_bin(cg.crs_with_intersect, "wi").hits, 1u);
    EXPECT_EQ(find_user_bin(cg.crs_with_intersect, "wiw").hits, 1u);

    cg.sample(4, 4); // y=4 fails y<3 in wi
    EXPECT_EQ(find_user_bin(cg.crs_with_intersect, "wi").hits, 1u);

    // Bug 2: fully ignored CrossQueueType bin has size 0; partially ignored has size 1
    EXPECT_EQ(find_user_bin(cg.crs_q_ignored, "q_full_ign").size, 0u);
    EXPECT_EQ(find_user_bin(cg.crs_q_ignored, "q_part_ign").size, 1u);
    cg.sample(2, 2); // Hits q_part_ign and auto-bin
    EXPECT_EQ(find_user_bin(cg.crs_q_ignored, "q_part_ign").hits, 1u);
    EXPECT_EQ(cg.crs_q_ignored.get_inst_coverage(), 100.0);

    // Bug 2b: CrossQueueType fully covered by illegal_cross_bin(binsof) has size 0
    int covered = 0, total = 0;
    {
      cross_q_illegal_cg icg("icg", __FILE__, __LINE__, cntxt);
      EXPECT_EQ(find_user_bin(icg.crs_q_illegal, "q_ill").size, 0u);
      EXPECT_EQ(icg.crs_q_illegal.get_inst_coverage(covered, total), 0.0);
      EXPECT_EQ(total, 0);
    }

    // Bug 2c: cross_bin_array element shadowed by ignore_cross_bin has size 0
    EXPECT_EQ(find_user_bin(cg.crs_arr_ignored, "qa[0]").size, 0u);
    EXPECT_EQ(find_user_bin(cg.crs_arr_ignored, "qa[1]").size, 1u);
    cg.sample(3, 3);
    EXPECT_EQ(cg.crs_arr_ignored.get_inst_coverage(covered, total), 100.0);
    EXPECT_EQ(total, 1);

    // Minor 6b: cross_bin::iff(c1).iff(c2) requires both c1 && c2
    cg.c1 = true; cg.c2 = false;
    cg.sample(1, 1);
    EXPECT_EQ(find_user_bin(cg.crs_chained_iff, "pair").hits, 0u);
    cg.c1 = false; cg.c2 = true;
    cg.sample(1, 1);
    EXPECT_EQ(find_user_bin(cg.crs_chained_iff, "pair").hits, 0u);
    cg.c1 = true; cg.c2 = true;
    cg.sample(1, 1);
    EXPECT_EQ(find_user_bin(cg.crs_chained_iff, "pair").hits, 1u);
    EXPECT_EQ(cg.crs_chained_iff.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_remediation, cross_queue_with_on_non_enumerable_coverpoints) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_non_enum_queue_with_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(find_user_bin(cg.crs_non_enum_q, "q_impossible").size, 0u);
    EXPECT_EQ(find_user_bin(cg.crs_non_enum_q, "q_partial").size, 1u);

    int covered = 0, total = 0;
    EXPECT_EQ(cg.crs_non_enum_q.get_inst_coverage(covered, total), 0.0);
    EXPECT_EQ(total, 1);

    cg.sample(0, 1.0); // Fails q_partial predicate (x > 2 && y > 1.5) -> hits rest
    EXPECT_EQ(find_user_bin(cg.crs_non_enum_q, "q_partial").hits, 0u);
    EXPECT_EQ(find_user_bin(cg.crs_non_enum_q, "rest").hits, 1u);

    cg.sample(5, 2.0); // Passes q_partial -> 100% coverage!
    EXPECT_EQ(find_user_bin(cg.crs_non_enum_q, "q_partial").hits, 1u);
    EXPECT_EQ(cg.crs_non_enum_q.get_inst_coverage(covered, total), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}
