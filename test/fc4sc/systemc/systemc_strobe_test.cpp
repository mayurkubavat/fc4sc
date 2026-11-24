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
#include "fc4sc_systemc.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

class SystemCStrobeTest : public ::testing::Test {
protected:
  void SetUp() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
  void TearDown() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
};

class sysc_sig_strobe_cg : public covergroup {
public:
  CG_CONS(sysc_sig_strobe_cg,
          const sc_core::sc_signal<int>* sig = nullptr,
          const sc_core::sc_signal<bool>* en_sig = nullptr,
          bool enable_strobe = true) {
    sig_ptr = sig;
    en_ptr = en_sig;
    type_option().strobe = enable_strobe;
    set_iff([this]() -> bool { return en_ptr != nullptr && en_ptr->read(); }, "en_sig");
  };

  const sc_core::sc_signal<int>* sig_ptr = nullptr;
  const sc_core::sc_signal<bool>* en_ptr = nullptr;

  COVERPOINT(int, cvp_sig, (sig_ptr != nullptr ? sig_ptr->read() : 0)) {
    bin<int>("b0", 0),
    bin<int>("b1", 1),
    bin<int>("b2", 2),
    bin<int>("b3", 3)
  };
};

class sysc_param_strobe_cg : public covergroup {
public:
  CG_CONS(sysc_param_strobe_cg) {
    type_option().strobe = true;
  };

  int a = 0;
  int b = 0;

  SAMPLE_CG(a, b);

  COVERPOINT(int, cvp_a, a) {
    bin<int>("a0", 0),
    bin<int>("a1", 1),
    bin<int>("a2", 2)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  };
};

class sysc_types_query_cg : public covergroup {
public:
  CG_CONS(sysc_types_query_cg) {};

  sc_dt::sc_int<8> si = 0;
  sc_dt::sc_uint<4> su = 0;
  sc_dt::sc_bigint<72> bi = 0;
  sc_dt::sc_biguint<72> bu = 0;
  sc_dt::sc_bv<8> bv = 0;
  sc_dt::sc_lv<4> lv = "0000";

  COVERPOINT(sc_dt::sc_int<8>, cvp_si, si) {
    bin<sc_dt::sc_int<8>>("r", interval(sc_dt::sc_int<8>(-4), sc_dt::sc_int<8>(3))),
    ignore_bin<sc_dt::sc_int<8>>("ign", interval(sc_dt::sc_int<8>(-1), sc_dt::sc_int<8>(1)))
  };

  COVERPOINT(sc_dt::sc_uint<4>, cvp_su, su) {
    bin<sc_dt::sc_uint<4>>("u", interval(sc_dt::sc_uint<4>(0), sc_dt::sc_uint<4>(7))),
    illegal_bin<sc_dt::sc_uint<4>>("ill", sc_dt::sc_uint<4>(4))
  };

  COVERPOINT(sc_dt::sc_bigint<72>, cvp_bi, bi) {
    bin<sc_dt::sc_bigint<72>>("b", interval(sc_dt::sc_bigint<72>(10), sc_dt::sc_bigint<72>(12))),
    ignore_bin<sc_dt::sc_bigint<72>>("bi", sc_dt::sc_bigint<72>(11))
  };

  COVERPOINT(sc_dt::sc_biguint<72>, cvp_bu, bu) {
    bin<sc_dt::sc_biguint<72>>("u", interval(sc_dt::sc_biguint<72>(100), sc_dt::sc_biguint<72>(102)))
  };

  COVERPOINT(sc_dt::sc_bv<8>, cvp_bv, bv) {
    bin<sc_dt::sc_bv<8>>("wc", wildcard<sc_dt::sc_bv<8>>("8'b0000_10??"))
  };

  COVERPOINT(sc_dt::sc_lv<4>, cvp_lv, lv) {
    bin<sc_dt::sc_lv<4>>("xz", sc_dt::sc_lv<4>("000X")),
    bin<sc_dt::sc_lv<4>>("one", sc_dt::sc_lv<4>("0001"))
  };
};

struct delta_driver_mod : public sc_core::sc_module {
  sc_core::sc_signal<int> sig{"sig"};
  sc_core::sc_signal<bool> en{"en"};
  sysc_sig_strobe_cg* cg = nullptr;
  int hit_during_delta = -1;

  SC_CTOR(delta_driver_mod) {
    SC_THREAD(run);
  }

  void run() {
    en.write(true);
    sig.write(0);
    sc_core::wait(10, sc_core::SC_NS);

    // At t=10ns delta 0: schedule strobe twice across deltas while updating sig 1 -> 2
    sig.write(1);
    cg->sample();
    hit_during_delta = static_cast<int>(cg->cvp_sig.get_bin_hit_count(1) + cg->cvp_sig.get_bin_hit_count(2));
    sc_core::wait(sc_core::SC_ZERO_TIME);
    // Still t=10ns (delta 1): sig is now 1, write 2 and sample again -> must coalesce to 1 hit of value 2!
    sig.write(2);
    cg->sample();

    sc_core::wait(10, sc_core::SC_NS);
    // At t=20ns: b2 must have been hit exactly ONCE, b1 0 times!
    sig.write(3);
    cg->sample();
    sc_core::sc_pause();
  }
};

struct stop_driver_mod : public sc_core::sc_module {
  sc_core::sc_signal<int> sig{"sig"};
  sc_core::sc_signal<bool> en{"en"};
  sysc_sig_strobe_cg* cg = nullptr;

  SC_CTOR(stop_driver_mod) {
    SC_THREAD(run);
  }

  void run() {
    en.write(true);
    sig.write(3);
    sc_core::wait(5, sc_core::SC_NS);
    cg->sample();
    sc_core::sc_stop();
  }
};

} // namespace

TEST_F(SystemCStrobeTest, sysc_types_uncovered_queries) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sysc_types_query_cg cg("cg", __FILE__, __LINE__, cntxt);
    auto si_ivs = cg.cvp_si.get_uncovered_intervals();
    ASSERT_EQ(si_ivs.size(), 2u);
    EXPECT_EQ(si_ivs[0].first, sc_dt::sc_int<8>(-4));
    EXPECT_EQ(si_ivs[0].second, sc_dt::sc_int<8>(-2));
    EXPECT_EQ(si_ivs[1].first, sc_dt::sc_int<8>(2));
    EXPECT_EQ(si_ivs[1].second, sc_dt::sc_int<8>(3));

    auto su_ivs = cg.cvp_su.get_uncovered_intervals();
    ASSERT_EQ(su_ivs.size(), 2u);
    EXPECT_EQ(su_ivs[0].second, sc_dt::sc_uint<4>(3));
    EXPECT_EQ(su_ivs[1].first, sc_dt::sc_uint<4>(5));

    auto bi_ivs = cg.cvp_bi.get_uncovered_intervals();
    ASSERT_EQ(bi_ivs.size(), 2u);
    EXPECT_EQ(bi_ivs[0].first, sc_dt::sc_bigint<72>(10));
    EXPECT_EQ(bi_ivs[1].first, sc_dt::sc_bigint<72>(12));

    auto bv_ivs = cg.cvp_bv.get_uncovered_intervals();
    ASSERT_EQ(bv_ivs.size(), 1u);
    EXPECT_EQ(bv_ivs[0].first.to_uint64(), 8u);
    EXPECT_EQ(bv_ivs[0].second.to_uint64(), 11u);

    auto lv_bins = cg.cvp_lv.get_uncovered_bins();
    ASSERT_EQ(lv_bins.size(), 2u);
    EXPECT_EQ(lv_bins[0], "xz");
  }
  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCStrobeTest, pre_timestep_dedup_and_pre_pause_resume) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    delta_driver_mod mod("mod");
    sysc_sig_strobe_cg cg("cg", __FILE__, __LINE__, cntxt, &mod.sig, &mod.en, true);
    mod.cg = &cg;

    sc_core::sc_start(); // runs until sc_pause() at t=20ns

    EXPECT_EQ(mod.hit_during_delta, 0);
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(0), 0u);
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(1), 0u);
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(2), 1u);
    // SC_PRE_PAUSE must have flushed the sample at t=20ns for sig==3
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(3), 1u);

    // Resume after sc_pause() and schedule another strobe
    mod.sig.write(0);
    cg.sample();
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(0), 1u);

    xml_printer::coverage_save("sysc_strobe_ucis.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream in("sysc_strobe_ucis.xml");
  ASSERT_TRUE(in.good());
  std::stringstream ss;
  ss << in.rdbuf();
  EXPECT_EQ(ss.str().find("strobe="), std::string::npos);
}

TEST_F(SystemCStrobeTest, parameterized_sample_coalesce_stop_iff_and_destructor) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sc_core::sc_signal<int> sig("sig");
    sc_core::sc_signal<bool> en("en");
    en.write(true);
    sig.write(0);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    sysc_param_strobe_cg pcg("pcg", __FILE__, __LINE__, cntxt);
    pcg.sample(0, 0);
    pcg.sample(1, 0);
    pcg.sample(2, 1); // last args win when timestep ends!
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(pcg.cvp_a.get_bin_hit_count(0), 0u);
    EXPECT_EQ(pcg.cvp_a.get_bin_hit_count(1), 0u);
    EXPECT_EQ(pcg.cvp_a.get_bin_hit_count(2), 1u);
    EXPECT_EQ(pcg.cvp_b.get_bin_hit_count(1), 1u);

    // Destroy covergroup while strobe is pending -> must cancel safely
    {
      auto transient_cg = std::unique_ptr<sysc_sig_strobe_cg>(
          new sysc_sig_strobe_cg("transient", __FILE__, __LINE__, cntxt, &sig, &en, true));
      transient_cg->sample();
    }
    sc_core::sc_start(5, sc_core::SC_NS);

    // stop() prevents queued strobe from counting, and iff evaluated at flush
    sysc_sig_strobe_cg scg("scg", __FILE__, __LINE__, cntxt, &sig, &en, true);
    sig.write(1);
    scg.sample();
    scg.stop();
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(scg.cvp_sig.get_bin_hit_count(1), 0u);

    scg.start();
    // iff false at sample(), becomes true before end of timestep -> sampled!
    en.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    scg.sample();
    en.write(true);
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(scg.cvp_sig.get_bin_hit_count(1), 1u);

    // sc_start(SC_ZERO_TIME) + explicit sc_flush_strobes()
    sig.write(2);
    scg.sample();
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(scg.cvp_sig.get_bin_hit_count(2), 0u);
    fc4sc::sc_flush_strobes();
    EXPECT_EQ(scg.cvp_sig.get_bin_hit_count(2), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCStrobeTest, pre_stop_flushes_pending_strobes) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    stop_driver_mod mod("stop_mod");
    sysc_sig_strobe_cg cg("cg_stop", __FILE__, __LINE__, cntxt, &mod.sig, &mod.en, true);
    mod.cg = &cg;

    sc_core::sc_start();
    EXPECT_EQ(cg.cvp_sig.get_bin_hit_count(3), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

namespace {

class sysc_illegal_strobe_cg : public covergroup {
public:
  CG_CONS(sysc_illegal_strobe_cg) {
    type_option().strobe = true;
  };

  int v = 0;
  SAMPLE_CG(v);

  COVERPOINT(int, cvp_v, v) {
    bin<int>("ok", 1),
    illegal_bin<int>("bad", 99)
  };
};

} // namespace

TEST_F(SystemCStrobeTest, multi_reset_aba_re_registers_strobe_controller) {
  for (int iter = 0; iter < 8; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      sc_core::sc_signal<int> sig("sig");
      sc_core::sc_signal<bool> en("en");
      en.write(true);
      sig.write(1);
      sc_core::sc_start(sc_core::SC_ZERO_TIME);

      auto cg = std::unique_ptr<sysc_sig_strobe_cg>(
          new sysc_sig_strobe_cg("cg_reset", __FILE__, __LINE__, cntxt, &sig, &en, true));
      cg->sample();
      sc_core::sc_start(5, sc_core::SC_NS);
      EXPECT_EQ(cg->cvp_sig.get_bin_hit_count(1), 1u) << "Failed at reset iter " << iter;
    }
    fc4sc::global::delete_context(cntxt);
  }
}

TEST_F(SystemCStrobeTest, flush_pending_continues_after_illegal_bin_exception) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sc_core::sc_signal<int> sig("sig");
    sc_core::sc_signal<bool> en("en");
    en.write(true);
    sig.write(1);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    sysc_illegal_strobe_cg cg_bad("cg_bad", __FILE__, __LINE__, cntxt);
    sysc_sig_strobe_cg cg_good("cg_good", __FILE__, __LINE__, cntxt, &sig, &en, true);

    // 1. Explicit sc_flush_strobes()
    cg_bad.sample(99);
    cg_good.sample();
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(fc4sc::sc_flush_strobes());
#else
    EXPECT_THROW(fc4sc::sc_flush_strobes(), fc4sc::illegal_bin_sample_exception);
#endif
    EXPECT_EQ(cg_good.cvp_sig.get_bin_hit_count(1), 1u);

    // 2. Kernel SC_PRE_TIMESTEP flush during sc_start()
    sig.write(2);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    cg_bad.sample(99);
    cg_good.sample();
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(sc_core::sc_start(5, sc_core::SC_NS));
#else
    EXPECT_THROW(sc_core::sc_start(5, sc_core::SC_NS), fc4sc::illegal_bin_sample_exception);
#endif
    EXPECT_EQ(cg_good.cvp_sig.get_bin_hit_count(2), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}
