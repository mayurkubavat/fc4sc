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

// Intentionally include fc4sc.hpp BEFORE <systemc> to verify header independence.
#include "fc4sc.hpp"
#include <systemc>
#include "gtest/gtest.h"

namespace {

class sysc_order2_cg : public covergroup {
public:
  CG_CONS(sysc_order2_cg) {};

  sc_dt::sc_int<64> s64 = 0;
  sc_dt::sc_uint<64> u64 = 0;
  sc_dt::sc_bv<16> bv16 = 0;

  COVERPOINT(sc_dt::sc_int<64>, s64_cvp, s64) {
    bin<sc_dt::sc_int<64>>("t", tolerance(sc_dt::sc_int<64>(100), 10))
  };
  COVERPOINT(sc_dt::sc_uint<64>, u64_cvp, u64) {};
  COVERPOINT(sc_dt::sc_bv<16>, bv16_cvp, bv16) {
    bin<sc_dt::sc_bv<16>>("wc", wildcard<sc_dt::sc_bv<16>>("16'hA5??"))
  };
};

} // namespace

TEST(systemc_include_order, fc4sc_before_systemc) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sysc_order2_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cg.u64_cvp.size(), 64u);
    cg.s64 = 95;
    cg.u64 = 1ULL << 63;
    cg.bv16 = 0xA53C;
    cg.sample();
    EXPECT_EQ(cg.s64_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.bv16_cvp.get_bin_hit_count(0), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}
