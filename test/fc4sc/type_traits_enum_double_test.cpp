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

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum unscoped_state_e { ST_IDLE = 0, ST_RUN = 1, ST_ERR = 2 };

enum class signed_mode_e : int8_t { NEG2 = -2, NEG1 = -1, ZERO = 0, POS3 = 3 };

enum class unsigned_tag_e : uint16_t { T0 = 0, T100 = 100, TMAX = 65535 };

class enum_cover_cg : public covergroup {
public:
  CG_CONS(enum_cover_cg) {};

  unscoped_state_e st = ST_IDLE;
  signed_mode_e sm = signed_mode_e::ZERO;
  unsigned_tag_e ut = unsigned_tag_e::T0;

  COVERPOINT(unscoped_state_e, st_cvp, st) {
    bin<unscoped_state_e>("idle", ST_IDLE),
    bin<unscoped_state_e>("active", interval(ST_RUN, ST_ERR)),
    transition_bin<unscoped_state_e>("tr", trans<unscoped_state_e>(ST_IDLE, ST_RUN))
  };

  COVERPOINT(signed_mode_e, sm_cvp, sm) {
    bin<signed_mode_e>("neg", interval(signed_mode_e::NEG2, signed_mode_e::NEG1)),
    bin<signed_mode_e>("nonneg", interval(signed_mode_e::ZERO, signed_mode_e::POS3)),
    default_bin_array<signed_mode_e>("rest")
  };

  COVERPOINT(unsigned_tag_e, ut_cvp, ut) {
    bin<unsigned_tag_e>("t0", unsigned_tag_e::T0),
    bin<unsigned_tag_e>("hi", unsigned_tag_e::TMAX)
  };

  cross<unscoped_state_e, signed_mode_e> crs =
      cross<unscoped_state_e, signed_mode_e>(this, "crs", &st_cvp, &sm_cvp);
};

class double_cover_cg : public covergroup {
public:
  CG_CONS(double_cover_cg) {};

  double d = 0.0;
  float f = 0.0f;

  COVERPOINT(double, d_cvp, d) {
    bin<double>("zero", -0.0),
    bin<double>("unit", half_open_interval(0.0, 1.0)),
    bin_array<double>("split", 4, interval(2.0, 10.0)),
    default_bin<double>("other")
  };

  COVERPOINT(float, f_cvp, f) {
    bin<float>("neg", interval(-5.0f, -1.0f)),
    bin<float>("pos", interval(1.0f, 5.0f)),
    default_bin_array<float>("other_f")
  };
};

class double_no_bins_cg : public covergroup {
public:
  CG_CONS(double_no_bins_cg) {};
  double d = 0.0;
  COVERPOINT(double, d_cvp, d) {};
};

} // namespace

TEST(type_traits_enum_double, signed_and_unsigned_enums_and_xml_roundtrip) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    enum_cover_cg cg("cg", __FILE__, __LINE__, cntxt);
    cg.st = ST_IDLE;
    cg.sm = signed_mode_e::NEG2;
    cg.ut = unsigned_tag_e::TMAX;
    cg.sample();

    cg.st = ST_RUN;
    cg.sm = signed_mode_e::POS3;
    cg.ut = unsigned_tag_e::T100;
    cg.sample();

    // Default bin array on signed_mode_e hit at 10 (not in [-2:-1] or [0:3])
    cg.sm = static_cast<signed_mode_e>(-10);
    cg.sample();

    EXPECT_EQ(cg.st_cvp.get_inst_coverage(), 100);
    EXPECT_EQ(cg.sm_cvp.get_inst_coverage(), 100);
    EXPECT_EQ(cg.ut_cvp.get_bin_hit_count(1), 1u);

    xml_printer::coverage_save("enum_roundtrip.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  // Verify signed enum class serialized "-2" and "-1" (not 18446744073709551614!)
  std::ifstream in("enum_roundtrip.xml");
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string xml = ss.str();
  EXPECT_NE(xml.find("from=\"-2\""), std::string::npos);
  EXPECT_NE(xml.find("to =\"-1\""), std::string::npos);
  EXPECT_EQ(xml.find("184467440737095516"), std::string::npos);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    enum_cover_cg cg("cg", __FILE__, __LINE__, cntxt2);
    EXPECT_TRUE(fc4sc::load_coverage_db("enum_roundtrip.xml", cntxt2));
    EXPECT_EQ(cg.st_cvp.get_inst_coverage(), 100);
    EXPECT_EQ(cg.sm_cvp.get_inst_coverage(), 100);
    EXPECT_EQ(cg.ut_cvp.get_bin_hit_count(1), 1u);
  }
  fc4sc::global::delete_context(cntxt2);
}

TEST(type_traits_enum_double, double_intervals_negzero_nan_and_roundtrip) {
  const double qnan = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(bin<double>("nan_bin", qnan), std::invalid_argument);
  EXPECT_THROW(bin<double>("nan_intv", interval(0.0, qnan)), std::invalid_argument);
  EXPECT_THROW(half_open_interval(1.0, 1.0), std::invalid_argument);
  EXPECT_THROW(half_open_interval(2.0, 1.0), std::invalid_argument);

  auto cntxt = fc4sc::global::create_new_context();
  {
    double_cover_cg cg("cg", __FILE__, __LINE__, cntxt);
    // -0.0 and +0.0 canonicalize together
    cg.d = +0.0;
    cg.f = -2.5f;
    cg.sample();
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(1), 1u); // 0.0 is also in [0.0, 1.0)
    EXPECT_EQ(cg.f_cvp.get_bin_hit_count(0), 1u);

    // 1.0 misses half_open_interval(0.0, 1.0); nextafter(1.0, 0.0) hits it
    cg.d = 1.0;
    cg.f = 10.5f; // hits default_bin_array
    cg.sample();
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(1), 1u);

    cg.d = std::nextafter(1.0, 0.0);
    cg.sample();
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(1), 2u);

    // split [2.0, 10.0] into 4: [2, 4), [4, 6), [6, 8), [8, 10]
    cg.d = 2.0; cg.sample();
    cg.d = 4.0; cg.sample();
    cg.d = 6.0; cg.sample();
    cg.d = 10.0; cg.sample();
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(3), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(4), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(5), 1u);

    // NaN sample must miss cleanly without corrupting interval_map
    const uint64_t misses_before = cg.d_cvp.get_misses();
    cg.d = qnan;
    cg.sample();
    EXPECT_EQ(cg.d_cvp.get_misses(), misses_before + 1);
    EXPECT_EQ(cg.d_cvp.get_inst_coverage(), 100);

    xml_printer::coverage_save("double_roundtrip.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream in("double_roundtrip.xml");
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string xml = ss.str();
  EXPECT_NE(xml.find("<userAttr "), std::string::npos);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    double_cover_cg cg("cg", __FILE__, __LINE__, cntxt2);
    EXPECT_TRUE(fc4sc::load_coverage_db("double_roundtrip.xml", cntxt2));
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(1), 2u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(5), 1u);
    EXPECT_EQ(cg.f_cvp.get_bin_hit_count(0), 1u);
  }
  fc4sc::global::delete_context(cntxt2);

  // Non-discrete double coverpoint without explicit bins must throw
  auto cntxt3 = fc4sc::global::create_new_context();
  {
    double_no_bins_cg bad("bad", __FILE__, __LINE__, cntxt3);
    EXPECT_THROW(bad.d_cvp.size(), std::invalid_argument);
  }
  fc4sc::global::delete_context(cntxt3);
}
