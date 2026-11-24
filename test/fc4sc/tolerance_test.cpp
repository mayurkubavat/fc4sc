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
#include <limits>
#include <stdexcept>

namespace {

enum class tol_dummy_e { A, B };

static_assert(!fc4sc::fc4sc_type_traits<bool>::supports_tolerance,
              "bool must not support tolerance");
static_assert(!fc4sc::fc4sc_type_traits<tol_dummy_e>::supports_tolerance,
              "enum must not support tolerance");
static_assert(fc4sc::fc4sc_type_traits<int>::supports_tolerance,
              "int must support tolerance");
static_assert(fc4sc::fc4sc_type_traits<double>::supports_tolerance,
              "double must support tolerance");

class tolerance_cg : public covergroup {
public:
  CG_CONS(tolerance_cg) {};

  int s_val = 0;
  uint8_t u8_val = 0;
  double d_val = 0.0;

  COVERPOINT(int, s_cvp, s_val) {
    bin<int>("around_zero", tolerance(0, 5)),
    bin<int>("pct_15", percent_tolerance(15, 10)),
    ignore_bin<int>("ign_neg", tolerance(-100, 5)),
    illegal_bin<int>("ill_high", tolerance(1000, 2))
  };

  COVERPOINT(uint8_t, u8_cvp, u8_val) {
    bin<uint8_t>("low_clamp", tolerance<uint8_t>(5, 10)),
    bin<uint8_t>("high_clamp", tolerance<uint8_t>(250, 10)),
    bin_array<uint8_t>("arr", 2, tolerance<uint8_t>(200, 10))
  };

  COVERPOINT(double, d_cvp, d_val) {
    bin<double>("nom", tolerance(10.0, 0.5)),
    bin<double>("pct", percent_tolerance(100.0, 12.5))
  };
};

} // namespace

TEST(tolerance_bins, integral_and_boundary_clamping) {
  EXPECT_EQ(tolerance(10, 3), interval(7, 13));
  EXPECT_EQ(tolerance(10, 0), interval(10, 10));
  EXPECT_EQ(tolerance<uint8_t>(5, 10), interval<uint8_t>(0, 15));
  EXPECT_EQ(tolerance<uint8_t>(250, 10), interval<uint8_t>(240, 255));
  EXPECT_EQ(tolerance<int8_t>(-120, 20), interval<int8_t>(-128, -100));
  EXPECT_EQ(tolerance<int8_t>(120, 20), interval<int8_t>(100, 127));

  EXPECT_EQ(tolerance<int64_t>(std::numeric_limits<int64_t>::min(), 100),
            interval<int64_t>(std::numeric_limits<int64_t>::min(),
                              std::numeric_limits<int64_t>::min() + 100));
  EXPECT_EQ(tolerance<int64_t>(std::numeric_limits<int64_t>::max(), 100),
            interval<int64_t>(std::numeric_limits<int64_t>::max() - 100,
                              std::numeric_limits<int64_t>::max()));
  EXPECT_EQ(tolerance<uint64_t>(std::numeric_limits<uint64_t>::max(), 100),
            interval<uint64_t>(std::numeric_limits<uint64_t>::max() - 100,
                               std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(tolerance<uint64_t>(10ULL, std::numeric_limits<uint64_t>::max()),
            interval<uint64_t>(0ULL, std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(tolerance<int>(0, 1e30),
            interval<int>(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

TEST(tolerance_bins, percent_tolerance_integral_and_double) {
  EXPECT_EQ(percent_tolerance(15, 10), interval(14, 16));
  EXPECT_EQ(percent_tolerance(9, 10), interval(9, 9));
  EXPECT_EQ(percent_tolerance(-100, 10), interval(-110, -90));
  EXPECT_EQ(percent_tolerance(15, 12.5), interval(14, 16));
  EXPECT_EQ(percent_tolerance(16, 12.5), interval(14, 18));
  EXPECT_EQ(percent_tolerance<uint8_t>(200, 50), interval<uint8_t>(100, 255));
  EXPECT_EQ(percent_tolerance<int8_t>(10, 150), interval<int8_t>(-5, 25));
  EXPECT_EQ(percent_tolerance<uint8_t>(10, 150), interval<uint8_t>(0, 25));

  EXPECT_EQ(percent_tolerance<int64_t>(std::numeric_limits<int64_t>::min(), 50),
            interval<int64_t>(std::numeric_limits<int64_t>::min(),
                              std::numeric_limits<int64_t>::min() / 2));
  EXPECT_EQ(percent_tolerance<uint64_t>(std::numeric_limits<uint64_t>::max(), 200),
            interval<uint64_t>(0ULL, std::numeric_limits<uint64_t>::max()));

  // Exact 64-bit discrete fractional percent_tolerance (no 53-bit long double loss)
  EXPECT_EQ(percent_tolerance<uint64_t>(std::numeric_limits<uint64_t>::max(), 12.5),
            interval<uint64_t>(16140901064495857664ULL,
                               std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(percent_tolerance<uint64_t>(std::numeric_limits<uint64_t>::max(), 1.5),
            interval<uint64_t>(18170042912603908341ULL,
                               std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(percent_tolerance<uint64_t>(900719925474099300ULL, 1.5),
            interval<uint64_t>(887209126591987811ULL, 914230724356210789ULL));
  EXPECT_EQ(percent_tolerance<uint64_t>(9223372036854775908ULL, 12.5),
            interval<uint64_t>(8070450532247928920ULL, 10376293541461622896ULL));
  EXPECT_EQ(percent_tolerance<int64_t>(std::numeric_limits<int64_t>::max(), 12.5),
            interval<int64_t>(8070450532247928832LL,
                              std::numeric_limits<int64_t>::max()));
  EXPECT_EQ(percent_tolerance<int64_t>(-900719925474099300LL, 1.5),
            interval<int64_t>(-914230724356210789LL, -887209126591987811LL));
  EXPECT_EQ(percent_tolerance<int64_t>(900719925474099300LL, 1.5),
            interval<int64_t>(887209126591987811LL, 914230724356210789LL));
  EXPECT_EQ(percent_tolerance<uint64_t>(10ULL, 1e-30), interval<uint64_t>(10ULL, 10ULL));
  EXPECT_EQ(percent_tolerance<uint64_t>(10ULL, 1e30),
            interval<uint64_t>(0ULL, std::numeric_limits<uint64_t>::max()));

  EXPECT_EQ(tolerance(10.0, 0.25), interval(9.75, 10.25));
  EXPECT_EQ(percent_tolerance(100.0, 12.5), interval(87.5, 112.5));
  EXPECT_EQ(percent_tolerance(std::numeric_limits<double>::infinity(), 0.0),
            interval(std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity()));
}

TEST(tolerance_bins, invalid_arguments_throw) {
  EXPECT_THROW(tolerance(10, -1), std::invalid_argument);
  EXPECT_THROW(tolerance(10.0, -0.1), std::invalid_argument);
  EXPECT_THROW(percent_tolerance(10, -1), std::invalid_argument);
  EXPECT_THROW(percent_tolerance(10.0, -0.5), std::invalid_argument);

  const double qnan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_THROW(tolerance(qnan, 1.0), std::invalid_argument);
  EXPECT_THROW(tolerance(1.0, qnan), std::invalid_argument);
  EXPECT_THROW(percent_tolerance(qnan, 10.0), std::invalid_argument);
  EXPECT_THROW(percent_tolerance(10.0, qnan), std::invalid_argument);
  EXPECT_THROW(tolerance(inf, inf), std::invalid_argument);
  EXPECT_THROW(percent_tolerance(inf, 10.0), std::invalid_argument);
}

TEST(tolerance_bins, coverpoint_sampling_and_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    tolerance_cg cg("cg", __FILE__, __LINE__, cntxt);
    cg.s_val = -5;
    cg.u8_val = 0;
    cg.d_val = 9.6;
    cg.sample();
    EXPECT_EQ(cg.s_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.u8_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(0), 1u);

    cg.s_val = 14;
    cg.u8_val = 255;
    cg.d_val = 87.5;
    cg.sample();
    EXPECT_EQ(cg.s_cvp.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cg.u8_cvp.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cg.d_cvp.get_bin_hit_count(1), 1u);

    cg.s_val = 1002;
    EXPECT_THROW(cg.sample(), fc4sc::illegal_bin_sample_exception);

    xml_printer::coverage_save("tolerance_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);
}
