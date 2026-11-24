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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class auto_color_e : uint8_t { Red = 1, Green = 4, Blue = 9 };
FC4SC_ENUM_VALUES(auto_color_e, auto_color_e::Red, auto_color_e::Green, auto_color_e::Blue)

enum class auto_signed_e : int8_t { Neg = -2, Zero = 0, Pos = 5 };

namespace {

class auto_default_cg : public covergroup {
public:
  CG_CONS(auto_default_cg) {};

  int x = 0;
  uint8_t u8 = 0;
  bool b = false;

  COVERPOINT(int, empty_cvp, x) {};
  int SAMPLE_POINT(z_val, zero_arg_cvp);
  coverpoint<int> zero_arg_cvp = coverpoint<int>(this, "zero_arg_cvp");
  COVERPOINT(uint8_t, u8_cvp, u8) {};
  COVERPOINT(bool, bool_cvp, b) {};
  COVERPOINT(int, empty_with_cvp, x) {
    bin<int>("impossible", 0).with([](int) { return false; })
  };
  COVERPOINT(int, default_only_cvp, x) {
    default_bin<int>("others")
  };
};

class auto_u8_custom_cg : public covergroup {
public:
  CG_CONS(auto_u8_custom_cg, unsigned int max_bins = 64) {
    option().auto_bin_max = max_bins;
  };

  uint8_t val = 0;
  COVERPOINT(uint8_t, cvp, val) {};
};

class auto_ignore_illegal_cg : public covergroup {
public:
  CG_CONS(auto_ignore_illegal_cg) {
    option().auto_bin_max = 4;
  };

  uint8_t val = 0;
  COVERPOINT(uint8_t, cvp, val) {
    ignore_bin<uint8_t>("ign_low", interval<uint8_t>(0, 63)),
    ignore_bin<uint8_t>("ign_mid", interval<uint8_t>(100, 127)),
    illegal_bin<uint8_t>("ill_top", 255)
  };
};

class auto_cross_cg : public covergroup {
public:
  CG_CONS(auto_cross_cg) {
    option().auto_bin_max = 4;
  };

  uint8_t a = 0;
  uint8_t b = 0;

  COVERPOINT(uint8_t, cvp_a, a) {};
  COVERPOINT(uint8_t, cvp_b, b) {};
  cross<uint8_t, uint8_t> ab = cross<uint8_t, uint8_t>(
      this, "ab", &cvp_a, &cvp_b,
      cross_bin("low_a", binsof<uint8_t>(&cvp_a).intersect(interval<uint8_t>(0, 63))));
};

class auto_cross_binname_cg : public covergroup {
public:
  CG_CONS(auto_cross_binname_cg) {
    option().auto_bin_max = 4;
  };

  uint8_t a = 0;
  uint8_t b = 0;

  COVERPOINT(uint8_t, cvp_a, a) {};
  COVERPOINT(uint8_t, cvp_b, b) {};
  cross<uint8_t, uint8_t> ab = cross<uint8_t, uint8_t>(
      this, "ab", &cvp_a, &cvp_b,
      cross_bin("q0", binsof<uint8_t>(&cvp_a, "auto[0:63]")));
};

class auto_cross_bad_binname_cg : public covergroup {
public:
  CG_CONS(auto_cross_bad_binname_cg) {
    option().auto_bin_max = 4;
  };

  uint8_t a = 0;
  uint8_t b = 0;

  COVERPOINT(uint8_t, cvp_a, a) {};
  COVERPOINT(uint8_t, cvp_b, b) {};
  cross<uint8_t, uint8_t> ab = cross<uint8_t, uint8_t>(
      this, "ab", &cvp_a, &cvp_b,
      cross_bin("bad", binsof<uint8_t>(&cvp_a, "no_such_auto_bin")));
};

class auto_enum_cg : public covergroup {
public:
  CG_CONS(auto_enum_cg) {
    option().auto_bin_max = 8;
  };

  auto_color_e color = auto_color_e::Red;
  auto_signed_e s_mode = auto_signed_e::Zero;

  COVERPOINT(auto_color_e, color_cvp, color) {};
  COVERPOINT(auto_color_e, color_ign_cvp, color) {
    ignore_bin<auto_color_e>("no_green", auto_color_e::Green)
  };
  COVERPOINT(auto_signed_e, signed_cvp, s_mode) {};
};

template <typename T>
class auto_wide_cg : public covergroup {
public:
  CG_CONS(auto_wide_cg, unsigned int max_bins = 64) {
    option().auto_bin_max = max_bins;
  };

  T val = T(0);
  COVERPOINT(T, cvp, val) {};
};

} // namespace

TEST(auto_bins, default_auto_bin_max_is_64_and_triggers) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_default_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.option().auto_bin_max, 64u);
    EXPECT_EQ(cg.empty_cvp.option().auto_bin_max, 64u);

    EXPECT_EQ(cg.empty_cvp.size(), 64u);
    EXPECT_EQ(cg.zero_arg_cvp.size(), 64u);
    EXPECT_EQ(cg.u8_cvp.size(), 64u);
    EXPECT_EQ(cg.bool_cvp.size(), 2u);
    EXPECT_EQ(cg.empty_with_cvp.size(), 0u);
    EXPECT_EQ(cg.default_only_cvp.size(), 0u);

    auto u8_bins = cg.u8_cvp.get_bins_base();
    ASSERT_EQ(u8_bins.size(), 64u);
    EXPECT_EQ(u8_bins[0]->name(), "auto[0:3]");
    EXPECT_EQ(u8_bins[63]->name(), "auto[252:255]");

    auto bool_bins = cg.bool_cvp.get_bins_base();
    ASSERT_EQ(bool_bins.size(), 2u);
    EXPECT_EQ(bool_bins[0]->name(), "auto[0]");
    EXPECT_EQ(bool_bins[1]->name(), "auto[1]");

    cg.x = 0;
    cg.u8 = 255;
    cg.b = true;
    cg.sample();

    xml_printer::coverage_save("auto_bins_default_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream in("auto_bins_default_test.xml");
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string xml = ss.str();
  EXPECT_NE(xml.find("auto_bin_max=\"64\""), std::string::npos);
  EXPECT_EQ(xml.find("auto_bin_max=\"10\""), std::string::npos);
}

TEST(auto_bins, inheritance_override_and_zero_throws) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_u8_custom_cg cg("cg", __FILE__, __LINE__, cntxt, 10);
    // 256 values across 10 bins: 9 bins of 25 ([0:24]..[200:224]), 1 bin of 31 ([225:255])
    EXPECT_EQ(cg.cvp.size(), 10u);
    auto bins = cg.cvp.get_bins_base();
    EXPECT_EQ(bins[0]->name(), "auto[0:24]");
    EXPECT_EQ(bins[8]->name(), "auto[200:224]");
    EXPECT_EQ(bins[9]->name(), "auto[225:255]");

    // Post-construction override before first sample updates auto-bins
    cg.option().auto_bin_max = 300;
    EXPECT_EQ(cg.cvp.size(), 256u);
    bins = cg.cvp.get_bins_base();
    EXPECT_EQ(bins[0]->name(), "auto[0]");
    EXPECT_EQ(bins[255]->name(), "auto[255]");

    // Child coverpoint override takes precedence
    cg.cvp.option().auto_bin_max = 4;
    EXPECT_EQ(cg.cvp.size(), 4u);
    cg.option().auto_bin_max = 16;
    EXPECT_EQ(cg.cvp.size(), 4u);

    // Zero auto_bin_max throws
    cg.cvp.option().auto_bin_max = 0;
    EXPECT_THROW(cg.cvp.size(), std::invalid_argument);
    cg.cvp.option().auto_bin_max = 4;
    EXPECT_EQ(cg.cvp.size(), 4u);

    // Once sampled, bins stay frozen
    cg.val = 10;
    cg.sample();
    EXPECT_EQ(cg.cvp.get_bin_hit_count(0), 1u);
    cg.cvp.option().auto_bin_max = 8;
    EXPECT_EQ(cg.cvp.size(), 4u);
    EXPECT_EQ(cg.cvp.get_bin_hit_count(0), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(auto_bins, ignore_and_illegal_subtraction) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_ignore_illegal_cg cg("cg", __FILE__, __LINE__, cntxt);
    // Raw bins: [0:63], [64:127], [128:191], [192:255]
    // [0:63] fully ignored -> dropped
    // [64:127] minus [100:127] -> [64:99]
    // [128:191] untouched
    // [192:255] minus 255 -> [192:254]
    ASSERT_EQ(cg.cvp.size(), 3u);
    auto bins = cg.cvp.get_bins_base();
    EXPECT_EQ(bins[0]->get_intervals_decimal()[0].first, "64");
    EXPECT_EQ(bins[0]->get_intervals_decimal()[0].second, "99");
    EXPECT_EQ(bins[1]->get_intervals_decimal()[0].first, "128");
    EXPECT_EQ(bins[1]->get_intervals_decimal()[0].second, "191");
    EXPECT_EQ(bins[2]->get_intervals_decimal()[0].first, "192");
    EXPECT_EQ(bins[2]->get_intervals_decimal()[0].second, "254");

    cg.val = 10; // ignored
    cg.sample();
    EXPECT_EQ(cg.cvp.get_inst_coverage(), 0);

    cg.val = 80;
    cg.sample();
    EXPECT_EQ(cg.cvp.get_bin_hit_count(0), 1u);

    cg.val = 255;
    EXPECT_THROW(cg.sample(), fc4sc::illegal_bin_sample_exception);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(auto_bins, signed_and_64bit_boundaries) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_wide_cg<int8_t> cg_i8("cg_i8", __FILE__, __LINE__, cntxt, 64);
    EXPECT_EQ(cg_i8.cvp.size(), 64u);
    auto i8_bins = cg_i8.cvp.get_bins_base();
    EXPECT_EQ(i8_bins.front()->get_intervals_decimal().front().first, "-128");
    EXPECT_EQ(i8_bins.back()->get_intervals_decimal().back().second, "127");

    auto_wide_cg<int64_t> cg_i64("cg_i64", __FILE__, __LINE__, cntxt, 7);
    EXPECT_EQ(cg_i64.cvp.size(), 7u);
    auto i64_bins = cg_i64.cvp.get_bins_base();
    EXPECT_EQ(i64_bins.front()->get_intervals_decimal().front().first,
              std::to_string(std::numeric_limits<int64_t>::min()));
    EXPECT_EQ(i64_bins.back()->get_intervals_decimal().back().second,
              std::to_string(std::numeric_limits<int64_t>::max()));
    cg_i64.val = std::numeric_limits<int64_t>::min();
    cg_i64.sample();
    cg_i64.val = std::numeric_limits<int64_t>::max();
    cg_i64.sample();
    EXPECT_EQ(cg_i64.cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg_i64.cvp.get_bin_hit_count(6), 1u);

    auto_wide_cg<uint64_t> cg_u64("cg_u64", __FILE__, __LINE__, cntxt, 3);
    EXPECT_EQ(cg_u64.cvp.size(), 3u);
    auto u64_bins = cg_u64.cvp.get_bins_base();
    EXPECT_EQ(u64_bins.front()->get_intervals_decimal().front().first, "0");
    EXPECT_EQ(u64_bins.back()->get_intervals_decimal().back().second,
              std::to_string(std::numeric_limits<uint64_t>::max()));
    cg_u64.val = 0;
    cg_u64.sample();
    cg_u64.val = std::numeric_limits<uint64_t>::max();
    cg_u64.sample();
    EXPECT_EQ(cg_u64.cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg_u64.cvp.get_bin_hit_count(2), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(auto_bins, cross_merge_db_roundtrip_and_dynamic) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_cross_cg cg1("cg1", __FILE__, __LINE__, cntxt);
    auto_cross_cg cg2("cg2", __FILE__, __LINE__, cntxt);
    cg1.type_option().merge_instances = true;

    EXPECT_EQ(cg1.cvp_a.size(), 4u);
    EXPECT_EQ(cg1.cvp_b.size(), 4u);
    // 4*4=16 products, 4 of them in low_a (cvp_a[0] x cvp_b[0..3]), 12 auto cross bins -> 13 total
    int covered = 0, total = 0;
    EXPECT_EQ(cg1.ab.get_inst_coverage(covered, total), 0);
    EXPECT_EQ(total, 13);

    cg1.a = 10; cg1.b = 10; cg1.sample();  // hits low_a
    cg2.a = 70; cg2.b = 200; cg2.sample(); // hits auto cross <1,3>
    EXPECT_EQ(cg1.ab.get_coverage(covered, total), 200.0 / 13.0);
    EXPECT_EQ(covered, 2);
    EXPECT_EQ(total, 13);

    xml_printer::coverage_save("auto_bins_roundtrip.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    auto_cross_cg cg1("cg1", __FILE__, __LINE__, cntxt2);
    auto_cross_cg cg2("cg2", __FILE__, __LINE__, cntxt2);
    cg1.type_option().merge_instances = true;
    EXPECT_TRUE(fc4sc::load_coverage_db("auto_bins_roundtrip.xml", cntxt2));
    int covered = 0, total = 0;
    cg1.ab.get_coverage(covered, total);
    EXPECT_EQ(covered, 2);
    EXPECT_EQ(total, 13);
  }
  fc4sc::global::delete_context(cntxt2);

  // Dynamic covergroup with auto-binned coverpoint
  auto cntxt3 = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory fac("dyn_auto_cg");
    uint8_t sample_var = 0;
    auto df = fac.create_coverpoint<uint8_t()>("dv", [&]() { return sample_var; });
    fc4sc::dynamic_covergroup dcg(fac, "dcg", __FILE__, __LINE__, cntxt3);
    df.bind_sample(dcg);
    dcg.option().auto_bin_max = 8;
    EXPECT_EQ(dcg.get_coverpoint("dv").size(), 8u);
    sample_var = 250;
    dcg.sample();
    EXPECT_GT(dcg.get_inst_coverage(), 0.0);
  }
  fc4sc::global::delete_context(cntxt3);
}

TEST(auto_bins, enum_and_enum_class) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_enum_cg cg("cg", __FILE__, __LINE__, cntxt);
    ASSERT_EQ(cg.color_cvp.size(), 3u);
    auto cbins = cg.color_cvp.get_bins_base();
    EXPECT_EQ(cbins[0]->name(), "auto[Red]");
    EXPECT_EQ(cbins[1]->name(), "auto[Green]");
    EXPECT_EQ(cbins[2]->name(), "auto[Blue]");

    ASSERT_EQ(cg.color_ign_cvp.size(), 2u);
    auto ibins = cg.color_ign_cvp.get_bins_base();
    EXPECT_EQ(ibins[0]->name(), "auto[Red]");
    EXPECT_EQ(ibins[1]->name(), "auto[Blue]");

    // Unregistered signed enum class falls back to int8_t [-128:127] split into 8 bins
    ASSERT_EQ(cg.signed_cvp.size(), 8u);
    auto sbins = cg.signed_cvp.get_bins_base();
    EXPECT_EQ(sbins[0]->get_intervals_decimal()[0].first, "-128");
    EXPECT_EQ(sbins[7]->get_intervals_decimal()[0].second, "127");

    cg.color = auto_color_e::Blue;
    cg.s_mode = auto_signed_e::Neg;
    cg.sample();
    EXPECT_EQ(cg.color_cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cg.signed_cvp.get_bin_hit_count(3), 1u);
    xml_printer::coverage_save("auto_bins_enum_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(auto_bins, cross_binsof_auto_name_with_cg_cons_auto_bin_max) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    auto_cross_binname_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.ab.size(), 16u);
    int covered = 0, total = 0;
    EXPECT_EQ(cg.ab.get_inst_coverage(covered, total), 0);
    EXPECT_EQ(total, 13);
    cg.a = 10;
    cg.b = 20;
    cg.sample();
    EXPECT_EQ(cg.ab.get_inst_coverage(covered, total), 100.0 / 13.0);
    EXPECT_EQ(covered, 1);

    auto_cross_bad_binname_cg bad_cg("bad_cg", __FILE__, __LINE__, cntxt);
    EXPECT_THROW(bad_cg.ab.size(), std::invalid_argument);
  }
  fc4sc::global::delete_context(cntxt);
}
