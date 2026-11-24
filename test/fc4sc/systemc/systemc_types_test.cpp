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

#include <systemc>
#include "fc4sc.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class sysc_types_cg : public covergroup {
public:
  CG_CONS(sysc_types_cg) {
    option().auto_bin_max = 8;
  };

  sc_dt::sc_int<8> si8 = 0;
  sc_dt::sc_uint<3> su3 = 0;
  sc_dt::sc_bigint<128> sbi128 = 0;
  sc_dt::sc_biguint<128> sbu128 = 0;
  sc_dt::sc_bv<128> bv128 = 0;
  sc_dt::sc_lv<4> lv4 = "0000";

  COVERPOINT(sc_dt::sc_int<8>, si8_cvp, si8) {
    bin<sc_dt::sc_int<8>>("neg", interval(sc_dt::sc_int<8>(-128), sc_dt::sc_int<8>(-1))),
    bin<sc_dt::sc_int<8>>("tol", tolerance(sc_dt::sc_int<8>(10), 5))
  };

  COVERPOINT(sc_dt::sc_uint<3>, su3_cvp, su3) {};

  COVERPOINT(sc_dt::sc_bigint<128>, sbi128_cvp, sbi128) {
    bin<sc_dt::sc_bigint<128>>("huge_neg",
        interval(sc_dt::sc_bigint<128>("-1000000000000000000000000"),
                 sc_dt::sc_bigint<128>("-999999999999999999999900"))),
    bin<sc_dt::sc_bigint<128>>("tol",
        tolerance(sc_dt::sc_bigint<128>("1000000000000000000000000"),
                  sc_dt::sc_bigint<128>(500)))
  };

  COVERPOINT(sc_dt::sc_biguint<128>, sbu128_cvp, sbu128) {
    bin_array<sc_dt::sc_biguint<128>>("arr", 4,
        interval(sc_dt::sc_biguint<128>(0),
                 ~sc_dt::sc_biguint<128>(0)))
  };

  COVERPOINT(sc_dt::sc_bv<128>, bv128_cvp, bv128) {
    bin_array<sc_dt::sc_bv<128>>("bv_arr", 4,
        interval(fc4sc_type_traits<sc_dt::sc_bv<128>>::min_value(),
                 fc4sc_type_traits<sc_dt::sc_bv<128>>::max_value()))
  };

  COVERPOINT(sc_dt::sc_lv<4>, lv4_cvp, lv4) {
    bin<sc_dt::sc_lv<4>>("zero", sc_dt::sc_lv<4>("0000")),
    bin<sc_dt::sc_lv<4>>("nibble", interval(sc_dt::sc_lv<4>("0001"), sc_dt::sc_lv<4>("1111"))),
    bin<sc_dt::sc_lv<4>>("xz", sc_dt::sc_lv<4>("10XZ")),
    bin<sc_dt::sc_lv<4>>("zzzz", sc_dt::sc_lv<4>("ZZZZ")),
    transition_bin<sc_dt::sc_lv<4>>("tr", trans<sc_dt::sc_lv<4>>(sc_dt::sc_lv<4>("0000"), sc_dt::sc_lv<4>("10XZ"))),
    default_bin_array<sc_dt::sc_lv<4>>("other_lv")
  };
};

class sysc_auto_cg : public covergroup {
public:
  CG_CONS(sysc_auto_cg) {
    option().auto_bin_max = 4;
  };

  sc_dt::sc_bigint<128> bi = 0;
  sc_dt::sc_lv<4> lv = "0000";
  sc_dt::sc_uint<8> u8 = 0;

  COVERPOINT(sc_dt::sc_bigint<128>, bi_cvp, bi) {};
  COVERPOINT(sc_dt::sc_lv<4>, lv_cvp, lv) {};
  COVERPOINT(sc_dt::sc_uint<8>, wc_cvp, u8) {
    bin<sc_dt::sc_uint<8>>("w", wildcard<sc_dt::sc_uint<8>>("8'b1010_xxxx"))
  };
};

} // namespace

TEST(systemc_types, all_datatypes_sampling_and_xml_roundtrip) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sysc_types_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.su3_cvp.size(), 8u);
    EXPECT_EQ(cg.sbu128_cvp.size(), 4u);
    EXPECT_EQ(cg.bv128_cvp.size(), 4u);

    cg.si8 = -10;
    cg.su3 = 7;
    cg.sbi128 = sc_dt::sc_bigint<128>("-1000000000000000000000000");
    cg.sbu128 = 0;
    cg.bv128 = fc4sc_type_traits<sc_dt::sc_bv<128>>::max_value();
    cg.lv4 = "0000";
    cg.sample();

    // Ensure 4-state X/Z does not match 2-state range [0001:1111]
    cg.si8 = 0;
    cg.su3 = 0;
    cg.sbi128 = 0;
    cg.sbu128 = ~sc_dt::sc_biguint<128>(0);
    cg.bv128 = 0;
    cg.lv4 = "00X0";
    cg.sample();
    EXPECT_EQ(cg.lv4_cvp.get_bin_hit_count(1), 0u); // nibble must remain 0

    // Sample consecutive transition 0000 => 10XZ and hit 10XZ point bin
    cg.lv4 = "0000";
    cg.sample();
    cg.lv4 = "10XZ";
    cg.sample();
    EXPECT_EQ(cg.lv4_cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cg.lv4_cvp.get_bin_hit_count(4), 1u);

    xml_printer::coverage_save("sysc_types_roundtrip.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream in("sysc_types_roundtrip.xml");
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string xml = ss.str();
  EXPECT_NE(xml.find("10XZ"), std::string::npos);

  auto cntxt2 = fc4sc::global::create_new_context();
  {
    sysc_types_cg cg("cg", __FILE__, __LINE__, cntxt2);
    EXPECT_TRUE(fc4sc::load_coverage_db("sysc_types_roundtrip.xml", cntxt2));
    EXPECT_EQ(cg.si8_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.su3_cvp.get_bin_hit_count(7), 1u);
    EXPECT_EQ(cg.sbi128_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.sbu128_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.bv128_cvp.get_bin_hit_count(3), 1u);
    EXPECT_EQ(cg.lv4_cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cg.lv4_cvp.get_bin_hit_count(4), 1u);
  }
  fc4sc::global::delete_context(cntxt2);
}

TEST(systemc_types, auto_bins_tolerances_wildcards_and_xz_range_reject) {
  EXPECT_THROW(
      bin<sc_dt::sc_lv<4>>("bad", interval(sc_dt::sc_lv<4>("0000"), sc_dt::sc_lv<4>("000X"))),
      std::invalid_argument);
  EXPECT_THROW(
      bin<sc_dt::sc_lv<4>>("bad2", interval(sc_dt::sc_lv<4>("000Z"), sc_dt::sc_lv<4>("000X"))),
      std::invalid_argument);

  const auto bi_tol = tolerance(sc_dt::sc_bigint<128>("1000000000000000000000"), 100);
  EXPECT_EQ(bi_tol.first.to_string(), "999999999999999999900");
  EXPECT_EQ(bi_tol.second.to_string(), "1000000000000000000100");

  const auto bu_pct = percent_tolerance(sc_dt::sc_biguint<128>("1000"), 10);
  EXPECT_EQ(bu_pct.first.to_string(), "900");
  EXPECT_EQ(bu_pct.second.to_string(), "1100");

  // Large floating-point tolerance/percent_tolerance must not prematurely saturate
  const auto bu128_large_tol = tolerance(sc_dt::sc_biguint<128>(0), 2.0e38);
  EXPECT_EQ(bu128_large_tol.first, sc_dt::sc_biguint<128>(0));
  EXPECT_LT(bu128_large_tol.second, fc4sc_type_traits<sc_dt::sc_biguint<128>>::max_value());
  EXPECT_GT(bu128_large_tol.second, sc_dt::sc_biguint<128>("199999990000000000000000000000000000000"));

  const auto bu128_sat_tol = tolerance(sc_dt::sc_biguint<128>(0), 4.0e38);
  EXPECT_EQ(bu128_sat_tol.second, fc4sc_type_traits<sc_dt::sc_biguint<128>>::max_value());

  const auto bu256_large_tol = tolerance(sc_dt::sc_biguint<256>(0), 1.0e50);
  EXPECT_EQ(bu256_large_tol.first, sc_dt::sc_biguint<256>(0));
  EXPECT_LT(bu256_large_tol.second, fc4sc_type_traits<sc_dt::sc_biguint<256>>::max_value());
  EXPECT_GT(bu256_large_tol.second, sc_dt::sc_biguint<256>("99999990000000000000000000000000000000000000000000"));

  const auto bi128_large_pct = percent_tolerance(sc_dt::sc_bigint<128>(1), 1.0e14 + 0.5);
  EXPECT_EQ(bi128_large_pct.first.to_string(), "-999999999999");
  EXPECT_EQ(bi128_large_pct.second.to_string(), "1000000000001");

  auto cntxt = fc4sc::global::create_new_context();
  {
    sysc_auto_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.bi_cvp.size(), 4u);
    EXPECT_EQ(cg.lv_cvp.size(), 4u);
    cg.u8 = 0xA5;
    cg.lv = "1111";
    cg.sample();
    EXPECT_EQ(cg.wc_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.lv_cvp.get_bin_hit_count(3), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

int sc_main(int ac, char* av[]) {
  testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}
