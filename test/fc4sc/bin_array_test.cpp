/******************************************************************************

   Copyright 2003-2018 AMIQ Consulting s.r.l.
   Copyright 2020 NVIDIA Corporation

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
/******************************************************************************
   Original Authors: Teodor Vasilache and Dragos Dospinescu,
                     AMIQ Consulting s.r.l. (contributors@amiq.com)

               Date: 2018-Feb-20
******************************************************************************/

#include "fc4sc.hpp"
#include "gtest/gtest.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class bin_array_test : public covergroup {
public:

  CG_CONS(bin_array_test) {};

  int sample_point1 = 0;
  int sample_point2 = 0;

  void sample(const int& x) {
    this->sample_point1 = x;
    this->sample_point2 = x;
    sample();
  }

  // coverpoint with 10 one value bins
  COVERPOINT(int, cvp1, sample_point1) {
    bin_array<int>("[1:5]", 5, interval(1,5))
  };

  COVERPOINT(int, cvp2, sample_point1) {
    bin_array<int>("bin_arr_2", 10, interval(1,5))
  };

};

TEST(bin_array, edge_cases) {

  auto cntxt = fc4sc::global::create_new_context();

  bin_array_test cvg("cvg",__FILE__,__LINE__,cntxt);
  cvg.cvp2.stop();

  //cvp1 should have 5 bins -> {1}, {2}, {3}, {4} and {5}
  int num_bins = 5;
  int total = -1;
  int hit = -1;

  EXPECT_EQ(cvg.cvp1.get_inst_coverage(hit, total), 0);
  EXPECT_EQ(total, num_bins);
  EXPECT_EQ(hit, 0);

  for (int i = 1; i <= 5; i++) {
    cvg.sample(i);
    EXPECT_EQ(cvg.cvp1.get_inst_coverage(hit, total), (100/num_bins) * i);
    EXPECT_EQ(total, num_bins);
    EXPECT_EQ(hit, i);

    if (i == 2) {
      cvg.sample(6);
      EXPECT_EQ(cvg.cvp1.get_inst_coverage(hit, total), (100/num_bins) * i);
      EXPECT_EQ(total, num_bins);
      EXPECT_EQ(hit, i);
    }
  }

  // cvp2 should have 1 bin -> {[1:5]} since length (which is 10) > interval ( [1:5])
  cvg.cvp2.start();

  total = -1;
  hit = -1;

  EXPECT_EQ(cvg.cvp2.get_inst_coverage(hit,total), 0);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(hit, 0);

  cvg.sample(5);
  EXPECT_EQ(cvg.cvp2.get_inst_coverage(hit,total), 100);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(hit, 1);

  xml_printer::coverage_save("bin_array_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + ".xml",cntxt);
  fc4sc::global::delete_context(cntxt);

}

namespace {

using expected_bin_t = std::pair<std::string, std::pair<std::string, std::string>>;

void expect_bins(fc4sc::coverpoint_base& cvp, const std::vector<expected_bin_t>& expected) {
  auto* data = static_cast<fc4sc::coverpoint_base_data_model*>(cvp.get_data());
  ASSERT_EQ(data->bins_data.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(data->bins_data[i]->get_name(), expected[i].first) << "bin " << i;
    auto intvs = data->bins_data[i]->get_intervals_decimal();
    ASSERT_EQ(intvs.size(), 1u) << "bin " << i;
    EXPECT_EQ(intvs[0].first, expected[i].second.first) << "bin " << i;
    EXPECT_EQ(intvs[0].second, expected[i].second.second) << "bin " << i;
  }
}

class remainder_cg : public covergroup {
public:
  CG_CONS(remainder_cg) {};

  int v3 = 0;
  int v4 = 0;
  int v2 = 0;
  int v1 = 0;

  COVERPOINT(int, rem3, v3) {
    bin_array<int>("rem3", 3, interval(1, 5))
  };
  COVERPOINT(int, rem4, v4) {
    bin_array<int>("rem4", 4, interval(0, 14))
  };
  COVERPOINT(int, rem2, v2) {
    bin_array<int>("rem2", 2, interval(0, 6))
  };
  COVERPOINT(int, rem1, v1) {
    bin_array<int>("rem1", 3, interval(0, 9))
  };
};

class s32_cg : public covergroup {
public:
  CG_CONS(s32_cg) {};

  int32_t v4 = 0;
  int32_t v2 = 0;
  int32_t vtop = 0;

  COVERPOINT(int32_t, s32_4, v4) {
    bin_array<int32_t>("s32_4", 4, interval<int32_t>(INT32_MIN, INT32_MAX))
  };
  COVERPOINT(int32_t, s32_2, v2) {
    bin_array<int32_t>("s32_2", 2, interval<int32_t>(INT32_MIN, INT32_MAX))
  };
  COVERPOINT(int32_t, s32_top, vtop) {
    bin_array<int32_t>("s32_top", 2, interval<int32_t>(INT32_MAX - 3, INT32_MAX))
  };
};

class u32_cg : public covergroup {
public:
  CG_CONS(u32_cg) {};

  uint32_t v = 0;

  COVERPOINT(uint32_t, u32_4, v) {
    bin_array<uint32_t>("u32_4", 4, interval<uint32_t>(0, UINT32_MAX))
  };
};

class full64_cg : public covergroup {
public:
  CG_CONS(full64_cg) {};

  uint64_t vu2 = 0;
  uint64_t vu3 = 0;
  uint64_t vu1 = 0;
  int64_t vs3 = 0;

  COVERPOINT(uint64_t, u64_2, vu2) {
    bin_array<uint64_t>("u64_2", 2, interval<uint64_t>(0, UINT64_MAX))
  };
  COVERPOINT(uint64_t, u64_3, vu3) {
    bin_array<uint64_t>("u64_3", 3, interval<uint64_t>(0, UINT64_MAX))
  };
  COVERPOINT(uint64_t, u64_1, vu1) {
    bin_array<uint64_t>("u64_1", 1, interval<uint64_t>(0, UINT64_MAX))
  };
  COVERPOINT(int64_t, s64_3, vs3) {
    bin_array<int64_t>("s64_3", 3, interval<int64_t>(INT64_MIN, INT64_MAX))
  };
};

class narrow_bool_cg : public covergroup {
public:
  CG_CONS(narrow_bool_cg) {};

  int8_t vs8_rem = 0;
  int8_t vs8_full = 0;
  uint8_t vu8_rem = 0;
  int16_t vs16_rem = 0;
  bool vb2 = false;

  COVERPOINT(int8_t, s8_rem, vs8_rem) {
    bin_array<int8_t>("s8_rem", 2, interval<int8_t>(-128, 126))
  };
  COVERPOINT(int8_t, s8_full, vs8_full) {
    bin_array<int8_t>("s8_full", 2, interval<int8_t>(-128, 127))
  };
  COVERPOINT(uint8_t, u8_rem, vu8_rem) {
    bin_array<uint8_t>("u8_rem", 2, interval<uint8_t>(0, 254))
  };
  COVERPOINT(int16_t, s16_rem, vs16_rem) {
    bin_array<int16_t>("s16_rem", 2, interval<int16_t>(INT16_MIN, INT16_MAX - 1))
  };
  COVERPOINT(bool, b2, vb2) {
    bin_array<bool>("b2", 2, interval<bool>(false, true))
  };
};

class rev_single_cg : public covergroup {
public:
  CG_CONS(rev_single_cg) {};

  int vrev = 0;
  int64_t vone = 0;

  COVERPOINT(int, rev, vrev) {
    bin_array<int>("rev", 3, interval(5, 1))
  };
  COVERPOINT(int64_t, one_val, vone) {
    bin_array<int64_t>("one_val", 1, interval<int64_t>(INT64_MIN, INT64_MIN))
  };
};

} // namespace

TEST(bin_array, remainder_placement_last_bin) {
  auto cntxt = fc4sc::global::create_new_context();
  remainder_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.rem3, {
    {"rem3[0]", {"1", "1"}},
    {"rem3[1]", {"2", "2"}},
    {"rem3[2]", {"3", "5"}}
  });
  expect_bins(cg.rem4, {
    {"rem4[0]", {"0", "2"}},
    {"rem4[1]", {"3", "5"}},
    {"rem4[2]", {"6", "8"}},
    {"rem4[3]", {"9", "14"}}
  });
  expect_bins(cg.rem2, {
    {"rem2[0]", {"0", "2"}},
    {"rem2[1]", {"3", "6"}}
  });
  expect_bins(cg.rem1, {
    {"rem1[0]", {"0", "2"}},
    {"rem1[1]", {"3", "5"}},
    {"rem1[2]", {"6", "9"}}
  });

  cg.v3 = 1; cg.v4 = 0; cg.v2 = 0; cg.v1 = 0; cg.sample();
  cg.v3 = 2; cg.v4 = 3; cg.v2 = 3; cg.v1 = 3; cg.sample();
  cg.v3 = 3; cg.v4 = 6; cg.v2 = 6; cg.v1 = 9; cg.sample();
  cg.v3 = 5; cg.v4 = 14; cg.sample();

  EXPECT_EQ(cg.rem3.get_bin_hit_count(0), 1u);
  EXPECT_EQ(cg.rem3.get_bin_hit_count(1), 1u);
  EXPECT_EQ(cg.rem3.get_bin_hit_count(2), 2u);
  EXPECT_EQ(cg.rem3.get_inst_coverage(), 100);
  EXPECT_EQ(cg.rem4.get_inst_coverage(), 100);
  EXPECT_EQ(cg.rem2.get_inst_coverage(), 100);
  EXPECT_EQ(cg.rem1.get_inst_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}

TEST(bin_array, signed_32bit_full_range_and_boundaries) {
  auto cntxt = fc4sc::global::create_new_context();
  s32_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.s32_4, {
    {"s32_4[0]", {"-2147483648", "-1073741825"}},
    {"s32_4[1]", {"-1073741824", "-1"}},
    {"s32_4[2]", {"0", "1073741823"}},
    {"s32_4[3]", {"1073741824", "2147483647"}}
  });
  expect_bins(cg.s32_2, {
    {"s32_2[0]", {"-2147483648", "-1"}},
    {"s32_2[1]", {"0", "2147483647"}}
  });
  expect_bins(cg.s32_top, {
    {"s32_top[0]", {std::to_string(INT32_MAX - 3), std::to_string(INT32_MAX - 2)}},
    {"s32_top[1]", {std::to_string(INT32_MAX - 1), std::to_string(INT32_MAX)}}
  });

  cg.v4 = INT32_MIN; cg.v2 = INT32_MIN; cg.vtop = INT32_MAX - 3; cg.sample();
  cg.v4 = -1073741824; cg.v2 = INT32_MAX; cg.vtop = INT32_MAX; cg.sample();
  cg.v4 = 0; cg.sample();
  cg.v4 = INT32_MAX; cg.sample();

  EXPECT_EQ(cg.s32_4.get_inst_coverage(), 100);
  EXPECT_EQ(cg.s32_2.get_inst_coverage(), 100);
  EXPECT_EQ(cg.s32_top.get_inst_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}

TEST(bin_array, unsigned_32bit_full_range) {
  auto cntxt = fc4sc::global::create_new_context();
  u32_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.u32_4, {
    {"u32_4[0]", {"0", "1073741823"}},
    {"u32_4[1]", {"1073741824", "2147483647"}},
    {"u32_4[2]", {"2147483648", "3221225471"}},
    {"u32_4[3]", {"3221225472", "4294967295"}}
  });

  cg.v = 0u; cg.sample();
  cg.v = 1073741824u; cg.sample();
  cg.v = 2147483648u; cg.sample();
  cg.v = UINT32_MAX; cg.sample();
  EXPECT_EQ(cg.u32_4.get_inst_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}

TEST(bin_array, full_64bit_ranges_signed_and_unsigned) {
  auto cntxt = fc4sc::global::create_new_context();
  full64_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.u64_2, {
    {"u64_2[0]", {"0", "9223372036854775807"}},
    {"u64_2[1]", {"9223372036854775808", "18446744073709551615"}}
  });
  expect_bins(cg.u64_3, {
    {"u64_3[0]", {"0", "6148914691236517204"}},
    {"u64_3[1]", {"6148914691236517205", "12297829382473034409"}},
    {"u64_3[2]", {"12297829382473034410", "18446744073709551615"}}
  });
  expect_bins(cg.u64_1, {
    {"u64_1[0]", {"0", "18446744073709551615"}}
  });
  expect_bins(cg.s64_3, {
    {"s64_3[0]", {"-9223372036854775808", "-3074457345618258604"}},
    {"s64_3[1]", {"-3074457345618258603", "3074457345618258601"}},
    {"s64_3[2]", {"3074457345618258602", "9223372036854775807"}}
  });

  cg.vu2 = 0ULL; cg.vu3 = 0ULL; cg.vu1 = UINT64_MAX; cg.vs3 = INT64_MIN; cg.sample();
  cg.vu2 = UINT64_MAX; cg.vu3 = 6148914691236517205ULL; cg.vs3 = 0; cg.sample();
  cg.vu3 = UINT64_MAX; cg.vs3 = INT64_MAX; cg.sample();

  EXPECT_EQ(cg.u64_2.get_inst_coverage(), 100);
  EXPECT_EQ(cg.u64_3.get_inst_coverage(), 100);
  EXPECT_EQ(cg.u64_1.get_inst_coverage(), 100);
  EXPECT_EQ(cg.s64_3.get_inst_coverage(), 100);

  xml_printer::coverage_save("bin_array_full_64bit_ranges_signed_and_unsigned.xml", cntxt);
  fc4sc::global::delete_context(cntxt);
}

TEST(bin_array, narrow_types_and_bool) {
  auto cntxt = fc4sc::global::create_new_context();
  narrow_bool_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.s8_rem, {
    {"s8_rem[0]", {"-128", "-2"}},
    {"s8_rem[1]", {"-1", "126"}}
  });
  expect_bins(cg.s8_full, {
    {"s8_full[0]", {"-128", "-1"}},
    {"s8_full[1]", {"0", "127"}}
  });
  expect_bins(cg.u8_rem, {
    {"u8_rem[0]", {"0", "126"}},
    {"u8_rem[1]", {"127", "254"}}
  });
  expect_bins(cg.s16_rem, {
    {"s16_rem[0]", {"-32768", "-2"}},
    {"s16_rem[1]", {"-1", "32766"}}
  });
  expect_bins(cg.b2, {
    {"b2[0]", {"0", "0"}},
    {"b2[1]", {"1", "1"}}
  });

  cg.vs8_rem = -128; cg.vs8_full = -128; cg.vu8_rem = 0; cg.vs16_rem = INT16_MIN; cg.vb2 = false; cg.sample();
  cg.vs8_rem = 126; cg.vs8_full = 127; cg.vu8_rem = 254; cg.vs16_rem = INT16_MAX - 1; cg.vb2 = true; cg.sample();

  EXPECT_EQ(cg.s8_rem.get_inst_coverage(), 100);
  EXPECT_EQ(cg.s8_full.get_inst_coverage(), 100);
  EXPECT_EQ(cg.u8_rem.get_inst_coverage(), 100);
  EXPECT_EQ(cg.s16_rem.get_inst_coverage(), 100);
  EXPECT_EQ(cg.b2.get_inst_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}

TEST(bin_array, reversed_bounds_and_single_value) {
  auto cntxt = fc4sc::global::create_new_context();
  rev_single_cg cg("cg", __FILE__, __LINE__, cntxt);

  expect_bins(cg.rev, {
    {"rev[0]", {"1", "1"}},
    {"rev[1]", {"2", "2"}},
    {"rev[2]", {"3", "5"}}
  });
  expect_bins(cg.one_val, {
    {"one_val[0]", {"-9223372036854775808", "-9223372036854775808"}}
  });

  cg.vrev = 1; cg.vone = INT64_MIN; cg.sample();
  cg.vrev = 2; cg.sample();
  cg.vrev = 5; cg.sample();
  EXPECT_EQ(cg.rev.get_inst_coverage(), 100);
  EXPECT_EQ(cg.one_val.get_inst_coverage(), 100);

  fc4sc::global::delete_context(cntxt);
}
