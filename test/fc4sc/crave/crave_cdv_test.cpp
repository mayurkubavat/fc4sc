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
#include "fc4sc_crave.hpp"
#include "gtest/gtest.h"

#include <cstdint>
#include <map>

namespace {

enum class cdv_op_e : int {
  NOP = 0,
  READ = 1,
  WRITE = 2,
  FLUSH = 5
};

} // namespace

FC4SC_ENUM_VALUES(cdv_op_e,
                  cdv_op_e::NOP,
                  cdv_op_e::READ,
                  cdv_op_e::WRITE,
                  cdv_op_e::FLUSH)

static_assert(fc4sc::is_crave_bridgeable<int>::value, "int must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<uint32_t>::value, "uint32_t must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<cdv_op_e>::value, "enum class must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_int<16>>::value, "sc_int<16> must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_uint<16>>::value, "sc_uint<16> must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_bigint<32>>::value, "sc_bigint<32> must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_biguint<32>>::value, "sc_biguint<32> must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_bv<16>>::value, "sc_bv<16> must be bridgeable");
static_assert(fc4sc::is_crave_bridgeable<sc_dt::sc_bv<96>>::value, "sc_bv<96> must be bridgeable");
static_assert(!fc4sc::is_crave_bridgeable<double>::value, "double must NOT be bridgeable");
static_assert(!fc4sc::is_crave_bridgeable<sc_dt::sc_lv<8>>::value, "sc_lv must NOT be bridgeable");
static_assert(!fc4sc::is_crave_bridgeable<sc_dt::sc_bigint<72>>::value, "sc_bigint<72> must NOT be bridgeable");
static_assert(!fc4sc::is_crave_bridgeable<sc_dt::sc_biguint<72>>::value, "sc_biguint<72> must NOT be bridgeable");

namespace {

class cdv_cov_cg : public covergroup {
public:
  CG_CONS(cdv_cov_cg) {};

  int addr = 0;
  int burst = 0;
  cdv_op_e op = cdv_op_e::NOP;
  sc_dt::sc_bv<96> wide_bv = 0;

  COVERPOINT(int, addr_cvp, addr) {
    bin_array<int>("a", 4, interval(0, 15)),
    ignore_bin<int>("ign", interval(4, 5))
  };

  COVERPOINT(int, burst_cvp, burst) {
    bin<int>("b1", 1),
    bin<int>("b2", 2),
    bin<int>("b4", 4),
    bin<int>("b8", 8)
  };

  COVERPOINT(cdv_op_e, op_cvp, op) {};

  COVERPOINT(sc_dt::sc_bv<96>, wide_cvp, wide_bv) {
    bin<sc_dt::sc_bv<96>>("lo", sc_dt::sc_bv<96>("0x000000000000000000000001")),
    bin<sc_dt::sc_bv<96>>("hi", interval(
        sc_dt::sc_bv<96>("0x000000010000000000000010"),
        sc_dt::sc_bv<96>("0x000000010000000000000013")))
  };

  cross<int, int> axb_crs = cross<int, int>(
      this, "axb_crs", &addr_cvp, &burst_cvp,
      cross_with([](int a, int b) { return !(a >= 12 && b == 8); }),
      ignore_cross_bin("ign_b8_a0", binsof<int>(&addr_cvp, "a[0]") && binsof<int>(&burst_cvp, "b8")));
};

class cdv_weighted_cg : public covergroup {
public:
  CG_CONS(cdv_weighted_cg) {};

  int val = 0;
  COVERPOINT(int, val_cvp, val) {
    bin<int>("low", 1),
    bin<int>("high", 2)
  };
};

class cdv_pkt_item : public crave::crv_sequence_item {
public:
  crave::crv_variable<int> addr;
  crave::crv_variable<int> burst;
  crave::crv_variable<int> op;
  crave::crv_variable<sc_dt::sc_bv<96>> wide_bv;

  crave::crv_constraint c_bounds {
    addr() >= 0, addr() <= 31,
    burst() >= 1, burst() <= 8,
    op() >= 0, op() <= 10
  };

  cdv_pkt_item(crave::crv_object_name name = "cdv_pkt_item") {}
};

class cdv_dist_item : public crave::crv_sequence_item {
public:
  crave::crv_variable<int> val;
  crave::crv_constraint c_dom { val() >= 1, val() <= 2 };

  cdv_dist_item(crave::crv_object_name name = "cdv_dist_item") {}
};

} // namespace

TEST(crave_cdv, closed_loop_intervals_cross_enum_and_wide_bv) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cdv_cov_cg cg("cdv_cg", __FILE__, __LINE__, cntxt);
    cdv_pkt_item item;

    for (int iter = 0; iter < 64; ++iter) {
      if (cg.addr_cvp.get_inst_coverage() < 100.0 ||
          cg.burst_cvp.get_inst_coverage() < 100.0 ||
          cg.op_cvp.get_inst_coverage() < 100.0 ||
          cg.wide_cvp.get_inst_coverage() < 100.0 ||
          cg.axb_crs.get_inst_coverage() < 100.0) {
        ASSERT_TRUE(item.randomize_with(
            fc4sc::hole_constraint(item.addr(), cg.addr_cvp),
            fc4sc::hole_constraint(item.burst(), cg.burst_cvp),
            fc4sc::hole_constraint(item.op(), cg.op_cvp),
            fc4sc::hole_constraint(item.wide_bv(), cg.wide_cvp),
            fc4sc::cross_hole_constraint(cg.axb_crs, item.addr(), item.burst(), 64)));
        cg.addr = item.addr;
        cg.burst = item.burst;
        cg.op = static_cast<cdv_op_e>(static_cast<int>(item.op));
        cg.wide_bv = item.wide_bv;
        cg.sample();
      } else {
        break;
      }
    }

    EXPECT_EQ(cg.addr_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.burst_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.op_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.wide_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.axb_crs.get_inst_coverage(), 100.0);

    // 100%-covered fallback: MUST remain SAT (not Constant(false))
    EXPECT_TRUE(item.randomize_with(
        fc4sc::hole_constraint(item.addr(), cg.addr_cvp),
        fc4sc::hole_constraint(item.op(), cg.op_cvp),
        fc4sc::hole_constraint(item.wide_bv(), cg.wide_cvp),
        fc4sc::cross_hole_constraint(cg.axb_crs, item.addr(), item.burst())));
#ifdef FC4SC_CRAVE_HAS_SOFT
    EXPECT_TRUE(item.randomize_with(
        fc4sc::soft_hole_constraint(item.burst(), cg.burst_cvp),
        fc4sc::soft_cross_hole_constraint(cg.axb_crs, item.addr(), item.burst())));
#endif
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(crave_cdv, weighted_deficit_distribution_and_soft_conflict_fallback) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cdv_weighted_cg wcg("wcg", __FILE__, __LINE__, cntxt);
    wcg.val_cvp.option().at_least = 20;
    // Hit val==1 19 times (deficit = 1), val==2 0 times (deficit = 20)
    wcg.val = 1;
    for (int i = 0; i < 19; ++i) wcg.sample();

    cdv_dist_item ditem;
    int cnt1 = 0;
    int cnt2 = 0;
    for (int i = 0; i < 200; ++i) {
      ASSERT_TRUE(ditem.randomize_with(fc4sc::hole_distribution(ditem.val(), wcg.val_cvp)));
      if (ditem.val == 1) ++cnt1;
      else if (ditem.val == 2) ++cnt2;
    }
    EXPECT_GT(cnt2, cnt1 * 3);

    // Finish covering val==2 -> 100%-covered hole_distribution must remain SAT
    wcg.val = 2;
    for (int i = 0; i < 20; ++i) wcg.sample();
    EXPECT_TRUE(ditem.randomize_with(fc4sc::hole_distribution(ditem.val(), wcg.val_cvp)));

#ifdef FC4SC_CRAVE_HAS_SOFT
    // Soft hole constraint overridden by conflicting hard constraint
    cdv_cov_cg cg("cg_soft", __FILE__, __LINE__, cntxt);
    cg.burst = 1;
    cg.sample(); // 1 is covered; 2,4,8 are holes
    cdv_pkt_item item;
    EXPECT_TRUE(item.randomize_with(
        fc4sc::soft_hole_constraint(item.burst(), cg.burst_cvp),
        item.burst() == 1));
    EXPECT_EQ(item.burst, 1);
#else
    cdv_cov_cg cg("cg_soft", __FILE__, __LINE__, cntxt);
    cg.burst = 1;
    cg.sample(); // 1 is covered; 2,4,8 are holes
    cdv_pkt_item item;
#endif

    // randomize_with_holes falls back cleanly when hard item constraint conflicts with holes
    EXPECT_TRUE(fc4sc::randomize_with_holes(item,
        fc4sc::hole_constraint(item.burst(), cg.burst_cvp),
        item.burst() == 3));
    EXPECT_EQ(item.burst, 3);
  }
  fc4sc::global::delete_context(cntxt);
}

namespace {

class cdv_bool_and_sysc_dist_cg : public covergroup {
public:
  CG_CONS(cdv_bool_and_sysc_dist_cg) {};

  bool flag = false;
  sc_dt::sc_int<8> si = 0;
  sc_dt::sc_uint<4> su = 0;
  sc_dt::sc_bv<16> bv = 0;

  COVERPOINT(bool, flag_cvp, flag) {
    bin<bool>("f", false),
    bin<bool>("t", true)
  };

  COVERPOINT(sc_dt::sc_int<8>, si_cvp, si) {
    bin<sc_dt::sc_int<8>>("neg", interval<sc_dt::sc_int<8>>(-8, -1)),
    bin<sc_dt::sc_int<8>>("pos", interval<sc_dt::sc_int<8>>(1, 8))
  };

  COVERPOINT(sc_dt::sc_uint<4>, su_cvp, su) {
    bin<sc_dt::sc_uint<4>>("lo", interval<sc_dt::sc_uint<4>>(0, 3)),
    bin<sc_dt::sc_uint<4>>("hi", interval<sc_dt::sc_uint<4>>(12, 15))
  };

  COVERPOINT(sc_dt::sc_bv<16>, bv_cvp, bv) {
    bin<sc_dt::sc_bv<16>>("lo", interval<sc_dt::sc_bv<16>>(1, 4)),
    bin<sc_dt::sc_bv<16>>("hi", interval<sc_dt::sc_bv<16>>(100, 103))
  };
};

class cdv_bool_sysc_dist_item : public crave::crv_sequence_item {
public:
  crave::crv_variable<bool> flag;
  crave::crv_variable<sc_dt::sc_int<8>> si;
  crave::crv_variable<sc_dt::sc_uint<4>> su;
  crave::crv_variable<sc_dt::sc_bv<16>> bv;

  cdv_bool_sysc_dist_item(crave::crv_object_name name = "cdv_bool_sysc_dist_item") {}
};

} // namespace

TEST(crave_cdv, hole_distribution_bool_and_systemc_types) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cdv_bool_and_sysc_dist_cg cg("bs_cg", __FILE__, __LINE__, cntxt);
    cdv_bool_sysc_dist_item item;

    // 1. bool hole_distribution: weighted deficit (false=19/20, true=0/20)
    cg.flag_cvp.option().at_least = 20;
    cg.flag = false;
    for (int i = 0; i < 19; ++i) cg.sample();
    int cnt_f = 0;
    int cnt_t = 0;
    for (int i = 0; i < 200; ++i) {
      ASSERT_TRUE(item.randomize_with(fc4sc::hole_distribution(item.flag(), cg.flag_cvp)));
      if (item.flag) ++cnt_t;
      else ++cnt_f;
    }
    EXPECT_GT(cnt_t, cnt_f * 3);

    // 2. prob = 1.0 when false is completely covered and true is uncovered
    cg.flag = false;
    cg.sample(); // false == 20 >= at_least(20)
    for (int i = 0; i < 20; ++i) {
      ASSERT_TRUE(item.randomize_with(fc4sc::hole_distribution(item.flag(), cg.flag_cvp)));
      EXPECT_TRUE(item.flag);
    }

    // 3. 100%-covered bool hole_distribution remains SAT and yields both values
    cg.flag = true;
    for (int i = 0; i < 20; ++i) cg.sample();
    bool saw_f = false, saw_t = false;
    for (int i = 0; i < 64; ++i) {
      ASSERT_TRUE(item.randomize_with(fc4sc::hole_distribution(item.flag(), cg.flag_cvp)));
      if (item.flag) saw_t = true;
      else saw_f = true;
    }
    EXPECT_TRUE(saw_f);
    EXPECT_TRUE(saw_t);

    // 4. prob = 0.0 when true is completely covered and false is uncovered
    cdv_bool_and_sysc_dist_cg cg_f("bf_cg", __FILE__, __LINE__, cntxt);
    cg_f.flag = true;
    cg_f.sample();
    for (int i = 0; i < 20; ++i) {
      ASSERT_TRUE(item.randomize_with(fc4sc::hole_distribution(item.flag(), cg_f.flag_cvp)));
      EXPECT_FALSE(item.flag);
    }

    // 5. SystemC sc_int<8>, sc_uint<4>, sc_bv<16> hole_distribution runtime convergence
    for (int iter = 0; iter < 32; ++iter) {
      if (cg.si_cvp.get_inst_coverage() < 100.0 ||
          cg.su_cvp.get_inst_coverage() < 100.0 ||
          cg.bv_cvp.get_inst_coverage() < 100.0) {
        ASSERT_TRUE(item.randomize_with(
            fc4sc::hole_distribution(item.si(), cg.si_cvp),
            fc4sc::hole_distribution(item.su(), cg.su_cvp),
            fc4sc::hole_distribution(item.bv(), cg.bv_cvp)));
        cg.si = item.si;
        cg.su = item.su;
        cg.bv = item.bv;
        cg.sample();
      } else {
        break;
      }
    }
    EXPECT_EQ(cg.si_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.su_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.bv_cvp.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}

int sc_main(int ac, char* av[]) {
  crave::set_global_seed(12345);
  testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}
