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

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace {

std::string ucis_xsd_path() {
  std::string file = __FILE__;
  const size_t pos = file.rfind("test/fc4sc/");
  if (pos != std::string::npos) {
    return file.substr(0, pos) + "ucis/UCIS.xsd";
  }
  return "../../ucis/UCIS.xsd";
}

void expect_valid_ucis_schema(const std::string& xml_path) {
  const std::string xsd = ucis_xsd_path();
  std::ifstream check(xsd.c_str());
  ASSERT_TRUE(check.good()) << "Missing schema at " << xsd;
  if (std::system("xmllint --version >/dev/null 2>&1") == 0) {
    const std::string cmd =
        "xmllint --schema \"" + xsd + "\" --noout \"" + xml_path + "\" >/dev/null 2>&1";
    EXPECT_EQ(std::system(cmd.c_str()), 0)
        << "UCIS.xsd schema validation failed for " << xml_path;
  }
}

class SystemCToggleTest : public ::testing::Test {
protected:
  void SetUp() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
  void TearDown() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
};

struct toggle_dut : public sc_core::sc_module {
  sc_core::sc_in<bool> in_b{"in_b"};
  sc_core::sc_out<uint8_t> out_u8{"out_u8"};
  sc_core::sc_inout<sc_dt::sc_uint<4>> io_u4{"io_u4"};
  sc_core::sc_signal<bool> sig_b{"sig_b"};
  sc_core::sc_buffer<int> buf_i{"buf_i"};
  sc_core::sc_signal_resolved res_l{"res_l"};
  sc_core::sc_signal_rv<4> rv_lv{"rv_lv"};
  sc_core::sc_signal<sc_dt::sc_bigint<72>> bi72{"bi72"};

  fc4sc::toggle_handle h_in;
  fc4sc::toggle_handle h_out;
  fc4sc::toggle_handle h_io;
  fc4sc::toggle_handle h_res;
  fc4sc::toggle_handle h_rv;
  fc4sc::toggle_handle h_bi;

  toggle_dut(sc_core::sc_module_name nm, fc4sc::global* cntxt)
      : sc_core::sc_module(nm) {
    fc4sc::toggle_options opts;
    opts.cntxt = cntxt;
    // Register ports BEFORE binding (deferred binding proof)
    h_in = fc4sc::track_toggle(in_b, opts);
    h_out = fc4sc::track_toggle(out_u8, opts);
    h_io = fc4sc::track_toggle(io_u4, opts);
    h_res = fc4sc::track_toggle(res_l, opts);
    h_rv = fc4sc::track_toggle(rv_lv, opts);
    h_bi = fc4sc::track_toggle(bi72, opts);
  }
};

struct glitch_mod : public sc_core::sc_module {
  sc_core::sc_signal<bool> sig_delta{"sig_delta"};
  sc_core::sc_signal<bool> sig_ts{"sig_ts"};

  SC_CTOR(glitch_mod) {
    SC_THREAD(run);
  }

  void run() {
    sig_delta.write(false);
    sig_ts.write(false);
    sc_core::wait(5, sc_core::SC_NS);
    // Same-timestep 0 -> 1 -> 0 glitch across deltas
    sig_delta.write(true);
    sig_ts.write(true);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    sig_delta.write(false);
    sig_ts.write(false);
    sc_core::wait(5, sc_core::SC_NS);
    // Clean 0 -> 1 transition
    sig_delta.write(true);
    sig_ts.write(true);
    sc_core::wait(5, sc_core::SC_NS);
    sc_core::sc_stop();
  }
};

} // namespace

TEST_F(SystemCToggleTest, signals_ports_4state_and_ucis_xml_validation) {
  auto cntxt = fc4sc::global::create_new_context();
  sc_core::sc_clock clk("clk", 10, sc_core::SC_NS);
  sc_core::sc_signal<bool> b_sig("b_sig");
  sc_core::sc_signal<uint8_t> u8_sig("u8_sig");
  sc_core::sc_signal<sc_dt::sc_uint<4>> u4_sig("u4_sig");

  toggle_dut dut("dut", cntxt);
  dut.in_b(b_sig);
  dut.out_u8(u8_sig);
  dut.io_u4(u4_sig);

  fc4sc::toggle_options clk_opts;
  clk_opts.cntxt = cntxt;
  auto h_clk = fc4sc::track_toggle(clk, clk_opts);

  dut.res_l.write(sc_dt::SC_LOGIC_0);
  dut.rv_lv.write(sc_dt::sc_lv<4>("0000"));
  dut.bi72.write(sc_dt::sc_bigint<72>(0));
  b_sig.write(false);
  u8_sig.write(0);
  u4_sig.write(0);

  sc_core::sc_start(1, sc_core::SC_NS);
  b_sig.write(true);
  u8_sig.write(0x05);
  u4_sig.write(0xA);
  dut.res_l.write(sc_dt::SC_LOGIC_X);
  dut.rv_lv.write(sc_dt::sc_lv<4>("01ZX"));
  dut.bi72.write(sc_dt::sc_bigint<72>(1) << 70);
  sc_core::sc_start(10, sc_core::SC_NS);

  b_sig.write(false);
  dut.res_l.write(sc_dt::SC_LOGIC_Z);
  dut.rv_lv.write(sc_dt::sc_lv<4>("0010"));
  sc_core::sc_start(10, sc_core::SC_NS);
  sc_core::sc_stop();

  EXPECT_EQ(dut.h_in.bit_count("0", "1", 0), 1u);
  EXPECT_EQ(dut.h_in.bit_count("1", "0", 0), 1u);
  EXPECT_EQ(dut.h_in.data()->port_direction, "in");
  EXPECT_EQ(dut.h_out.data()->port_direction, "out");
  EXPECT_EQ(dut.h_io.data()->port_direction, "inout");
  EXPECT_EQ(dut.h_out.bit_count("0", "1", 0), 1u);
  EXPECT_EQ(dut.h_out.bit_count("0", "1", 2), 1u);
  EXPECT_EQ(dut.h_out.bit_count("0", "1", 1), 0u);
  EXPECT_EQ(dut.h_res.bit_count("0", "X", 0), 1u);
  EXPECT_EQ(dut.h_res.bit_count("X", "Z", 0), 1u);
  EXPECT_EQ(dut.h_rv.bit_count("0", "X", 0), 1u);
  EXPECT_EQ(dut.h_rv.bit_count("X", "0", 0), 1u);
  EXPECT_EQ(dut.h_rv.bit_count("X", "0", 3), 0u);
  EXPECT_EQ(dut.h_rv.bit_count("0", "Z", 1), 1u);
  EXPECT_EQ(dut.h_rv.bit_count("Z", "1", 1), 1u);
  EXPECT_EQ(dut.h_bi.bit_count("0", "1", 70), 1u);
  EXPECT_GE(h_clk.bit_count("0", "1", 0), 1u);

  const std::string xml_path = "sysc_toggle_out.xml";
  xml_printer::coverage_save(xml_path, cntxt);
  expect_valid_ucis_schema(xml_path);

  // Merge two runs and validate XML
  const std::string merged_path = "sysc_toggle_merged.xml";
  fc4sc::merge_coverage_db({xml_path, xml_path}, merged_path);
  expect_valid_ucis_schema(merged_path);

  // Report integration
  const std::string md = fc4sc::generate_markdown_report(cntxt);
  const std::string html = fc4sc::generate_html_report(cntxt);
  EXPECT_NE(md.find("Toggle Coverage"), std::string::npos);
  EXPECT_NE(html.find("Toggle Coverage"), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCToggleTest, delta_vs_timestep_glitch_suppression_and_flush) {
  auto cntxt = fc4sc::global::create_new_context();
  glitch_mod mod("mod");

  fc4sc::toggle_options d_opts;
  d_opts.cntxt = cntxt;
  d_opts.sample_mode = fc4sc::toggle_sample_mode::delta;

  fc4sc::toggle_options ts_opts;
  ts_opts.cntxt = cntxt;
  ts_opts.sample_mode = fc4sc::toggle_sample_mode::timestep;

  auto hd = fc4sc::track_toggle(mod.sig_delta, d_opts);
  auto hts = fc4sc::track_toggle(mod.sig_ts, ts_opts);

  sc_core::sc_start();
  fc4sc::sc_flush_toggles();

  // Delta mode sees the 0->1->0 glitch + final 0->1 (two 0->1, one 1->0)
  EXPECT_EQ(hd.bit_count("0", "1", 0), 2u);
  EXPECT_EQ(hd.bit_count("1", "0", 0), 1u);
  // Timestep mode suppresses the same-time glitch and only sees final 0->1
  EXPECT_EQ(hts.bit_count("0", "1", 0), 1u);
  EXPECT_EQ(hts.bit_count("1", "0", 0), 0u);

  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCToggleTest, track_module_toggles_and_reset_aba) {
  {
    auto cntxt = fc4sc::global::create_new_context();
    toggle_dut dut("auto_dut", cntxt);
    sc_core::sc_signal<bool> b("b");
    sc_core::sc_signal<uint8_t> u8("u8");
    sc_core::sc_signal<sc_dt::sc_uint<4>> u4("u4");
    dut.in_b(b); dut.out_u8(u8); dut.io_u4(u4);
    fc4sc::toggle_options m_opts;
    m_opts.cntxt = cntxt;
    auto handles = fc4sc::track_module_toggles(dut, m_opts);
    EXPECT_GE(handles.size(), 4u); // in_b, out_u8, sig_b, buf_i, res_l
    b.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);
    dut.sig_b.write(true);
    dut.buf_i.write(1);
    dut.buf_i.write(2);
    sc_core::sc_start(1, sc_core::SC_NS);
    fc4sc::global::delete_context(cntxt);
  }

  for (int iter = 0; iter < 10; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      sc_core::sc_signal<bool> sig{"sig"};
      fc4sc::toggle_options opts;
      opts.cntxt = cntxt;
      auto h = fc4sc::track_toggle(sig, opts);
      sc_core::sc_start(1, sc_core::SC_NS);
      sig.write(true);
      sc_core::sc_start(1, sc_core::SC_NS);
      EXPECT_EQ(h.bit_count("0", "1", 0), 1u);
      // sig destroyed at scope exit before next reset()
    }
    fc4sc::global::delete_context(cntxt);
  }
}

namespace {

struct t0_and_rv_mod : public sc_core::sc_module {
  sc_core::sc_signal<bool> t0_bool_d{"t0_bool_d"};
  sc_core::sc_signal<bool> t0_bool_ts{"t0_bool_ts"};
  sc_core::sc_signal<sc_dt::sc_logic> t0_log_ts{"t0_log_ts"};
  sc_core::sc_signal<bool> d1_kick{"d1_kick"};
  sc_core::sc_signal_rv<4> rv_bus{"rv_bus"};
  sc_core::sc_signal<int> phase{"phase"};

  SC_CTOR(t0_and_rv_mod) {
    SC_METHOD(proc_t0);
    // runs at t=0 initialization
    SC_METHOD(proc_d1);
    sensitive << d1_kick;
    dont_initialize();
    SC_METHOD(drv_a);
    sensitive << phase;
    SC_METHOD(drv_b);
    sensitive << phase;
  }

  void proc_t0() {
    t0_bool_d.write(true);
    t0_bool_ts.write(true);
    t0_log_ts.write(sc_dt::SC_LOGIC_1);
    d1_kick.write(true); // triggers delta 1 at t=0
  }

  void proc_d1() {
    // Runs at t=0 delta 1 so t0_bool_ts/t0_log_ts must still retain delta-0 toggle
    (void)d1_kick.read();
  }

  void drv_a() {
    const int p = phase.read();
    if (p == 0) rv_bus.write(sc_dt::sc_lv<4>("ZZZZ"));
    else if (p == 1) rv_bus.write(sc_dt::sc_lv<4>("1111"));
    else rv_bus.write(sc_dt::sc_lv<4>("ZZZZ"));
  }

  void drv_b() {
    const int p = phase.read();
    if (p == 0) rv_bus.write(sc_dt::sc_lv<4>("0101"));
    else if (p == 1) rv_bus.write(sc_dt::sc_lv<4>("0000")); // contention -> XXXX
    else rv_bus.write(sc_dt::sc_lv<4>("1010"));
  }
};

} // namespace

TEST_F(SystemCToggleTest, pre_sc_start_init_zero_toggles_and_t0_writes_and_multi_driver_rv) {
  // Part 1: Pre-sc_start() .write(...) with NO subsequent transitions must yield 0 toggles
  {
    auto cntxt = fc4sc::global::create_new_context();
    sc_core::sc_signal<bool> b_d{"b_d"}, b_ts{"b_ts"};
    sc_core::sc_signal<sc_dt::sc_logic> l_d{"l_d"}, l_ts{"l_ts"};
    sc_core::sc_signal<sc_dt::sc_lv<4>> lv_d{"lv_d"}, lv_ts{"lv_ts"};
    sc_core::sc_signal_resolved res_d{"res_d"}, res_ts{"res_ts"};
    sc_core::sc_signal_rv<4> rv_d{"rv_d"}, rv_ts{"rv_ts"};

    fc4sc::toggle_options d_opts;
    d_opts.cntxt = cntxt;
    d_opts.sample_mode = fc4sc::toggle_sample_mode::delta;

    fc4sc::toggle_options ts_opts;
    ts_opts.cntxt = cntxt;
    ts_opts.sample_mode = fc4sc::toggle_sample_mode::timestep;

    auto hb_d = fc4sc::track_toggle(b_d, d_opts);
    auto hb_ts = fc4sc::track_toggle(b_ts, ts_opts);
    auto hl_d = fc4sc::track_toggle(l_d, d_opts);
    auto hl_ts = fc4sc::track_toggle(l_ts, ts_opts);
    auto hlv_d = fc4sc::track_toggle(lv_d, d_opts);
    auto hlv_ts = fc4sc::track_toggle(lv_ts, ts_opts);
    auto hres_d = fc4sc::track_toggle(res_d, d_opts);
    auto hres_ts = fc4sc::track_toggle(res_ts, ts_opts);
    auto hrv_d = fc4sc::track_toggle(rv_d, d_opts);
    auto hrv_ts = fc4sc::track_toggle(rv_ts, ts_opts);

    b_d.write(true);
    b_ts.write(true);
    l_d.write(sc_dt::SC_LOGIC_0);
    l_ts.write(sc_dt::SC_LOGIC_1);
    lv_d.write(sc_dt::sc_lv<4>("0101"));
    lv_ts.write(sc_dt::sc_lv<4>("1010"));
    res_d.write(sc_dt::SC_LOGIC_0);
    res_ts.write(sc_dt::SC_LOGIC_1);
    rv_d.write(sc_dt::sc_lv<4>("0000"));
    rv_ts.write(sc_dt::sc_lv<4>("1111"));

    sc_core::sc_start(2, sc_core::SC_NS);
    sc_core::sc_stop();
    fc4sc::sc_flush_toggles();

    EXPECT_EQ(hb_d.bit_count("0", "1", 0), 0u);
    EXPECT_EQ(hb_ts.bit_count("0", "1", 0), 0u);
    EXPECT_EQ(hl_d.bit_count("X", "0", 0), 0u);
    EXPECT_EQ(hl_ts.bit_count("X", "1", 0), 0u);
    EXPECT_EQ(hlv_d.bit_count("X", "0", 0), 0u);
    EXPECT_EQ(hlv_d.bit_count("X", "1", 0), 0u);
    EXPECT_EQ(hlv_ts.bit_count("X", "0", 0), 0u);
    EXPECT_EQ(hlv_ts.bit_count("X", "1", 1), 0u);
    EXPECT_EQ(hres_d.bit_count("X", "0", 0), 0u);
    EXPECT_EQ(hres_ts.bit_count("X", "1", 0), 0u);
    EXPECT_EQ(hrv_d.bit_count("X", "0", 0), 0u);
    EXPECT_EQ(hrv_ts.bit_count("X", "1", 0), 0u);

    fc4sc::global::delete_context(cntxt);
  }

  // Part 2: t=0 SC_METHOD delta-0 writes + multi-delta timestep + multi-driver sc_signal_rv
  sc_core::sc_get_curr_simcontext()->reset();
  {
    auto cntxt = fc4sc::global::create_new_context();
    t0_and_rv_mod mod("mod");

    fc4sc::toggle_options d_opts;
    d_opts.cntxt = cntxt;
    d_opts.sample_mode = fc4sc::toggle_sample_mode::delta;

    fc4sc::toggle_options ts_opts;
    ts_opts.cntxt = cntxt;
    ts_opts.sample_mode = fc4sc::toggle_sample_mode::timestep;

    auto ht0_d = fc4sc::track_toggle(mod.t0_bool_d, d_opts);
    auto ht0_ts = fc4sc::track_toggle(mod.t0_bool_ts, ts_opts);
    auto ht0_l = fc4sc::track_toggle(mod.t0_log_ts, ts_opts);
    auto hrv = fc4sc::track_toggle(mod.rv_bus, ts_opts);

    mod.phase.write(0); // drv_a=ZZZZ, drv_b=0101 -> rv_bus=0101 at t=0
    sc_core::sc_start(5, sc_core::SC_NS);

    // Genuine t=0 SC_METHOD delta-0 writes must still count!
    EXPECT_EQ(ht0_d.bit_count("0", "1", 0), 1u);
    EXPECT_EQ(ht0_ts.bit_count("0", "1", 0), 1u);
    EXPECT_EQ(ht0_l.bit_count("X", "1", 0), 1u);
    // rv_bus settled from X to 0101 via t=0 SC_METHOD drivers
    EXPECT_EQ(hrv.bit_count("X", "1", 0), 1u);
    EXPECT_EQ(hrv.bit_count("X", "0", 1), 1u);

    mod.phase.write(1); // drv_a=1111, drv_b=0000 -> XXXX (contention!)
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(hrv.bit_count("1", "X", 0), 1u);
    EXPECT_EQ(hrv.bit_count("0", "X", 1), 1u);

    mod.phase.write(2); // drv_a=ZZZZ, drv_b=1010 -> 1010
    sc_core::sc_start(5, sc_core::SC_NS);
    sc_core::sc_stop();
    fc4sc::sc_flush_toggles();
    EXPECT_EQ(hrv.bit_count("X", "0", 0), 1u);
    EXPECT_EQ(hrv.bit_count("X", "1", 1), 1u);

    const std::string rv_xml = "sysc_toggle_rv_multi.xml";
    xml_printer::coverage_save(rv_xml, cntxt);
    expect_valid_ucis_schema(rv_xml);

    fc4sc::global::delete_context(cntxt);
  }
}

namespace {

SC_MODULE(t0_overwrite_mod) {
  sc_core::sc_signal<bool> b_d{"b_d"}, b_ts{"b_ts"}, b_pre_d{"b_pre_d"};
  sc_core::sc_signal<sc_dt::sc_logic> l_d{"l_d"}, l_ts{"l_ts"};
  sc_core::sc_signal<sc_dt::sc_lv<4>> lv_d{"lv_d"};

  SC_CTOR(t0_overwrite_mod) {
    SC_METHOD(overwrite_t0);
  }

  void overwrite_t0() {
    b_d.write(false);
    b_ts.write(false);
    b_pre_d.write(false);
    l_d.write(sc_dt::SC_LOGIC_0);
    l_ts.write(sc_dt::SC_LOGIC_0);
    lv_d.write(sc_dt::sc_lv<4>("0000"));
  }
};

} // namespace

TEST_F(SystemCToggleTest, pre_sc_start_write_overwritten_at_t0_and_aba_reset) {
  // Verify pre-sc_start() .write(...) overwritten at t=0 delta 0 across 15x reset() ABA
  for (int iter = 0; iter < 15; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      t0_overwrite_mod mod("mod");

      fc4sc::toggle_options d_opts;
      d_opts.cntxt = cntxt;
      d_opts.sample_mode = fc4sc::toggle_sample_mode::delta;

      fc4sc::toggle_options ts_opts;
      ts_opts.cntxt = cntxt;
      ts_opts.sample_mode = fc4sc::toggle_sample_mode::timestep;

      // Write BEFORE track_toggle on b_pre_d, AFTER track_toggle on the rest
      mod.b_pre_d.write(true);
      auto hb_pre_d = fc4sc::track_toggle(mod.b_pre_d, d_opts);
      auto hb_d = fc4sc::track_toggle(mod.b_d, d_opts);
      auto hb_ts = fc4sc::track_toggle(mod.b_ts, ts_opts);
      auto hl_d = fc4sc::track_toggle(mod.l_d, d_opts);
      auto hl_ts = fc4sc::track_toggle(mod.l_ts, ts_opts);
      auto hlv_d = fc4sc::track_toggle(mod.lv_d, d_opts);

      mod.b_d.write(true);
      mod.b_ts.write(true);
      mod.l_d.write(sc_dt::SC_LOGIC_1);
      mod.l_ts.write(sc_dt::SC_LOGIC_1);
      mod.lv_d.write(sc_dt::sc_lv<4>("1111"));

      if (iter % 3 == 0) {
        sc_core::sc_start(sc_core::SC_ZERO_TIME);
        fc4sc::sc_flush_toggles();
      } else if (iter % 3 == 1) {
        sc_core::sc_start(0, sc_core::SC_NS);
        fc4sc::sc_flush_toggles();
      } else {
        sc_core::sc_start(2, sc_core::SC_NS);
        sc_core::sc_stop();
      }

      EXPECT_EQ(hb_pre_d.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(hb_d.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(hb_ts.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(hl_d.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(hl_d.bit_count("X", "0", 0), 0u) << "iter=" << iter;
      EXPECT_EQ(hl_ts.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(hl_ts.bit_count("X", "0", 0), 0u) << "iter=" << iter;
      for (unsigned int b = 0; b < 4; ++b) {
        EXPECT_EQ(hlv_d.bit_count("1", "0", b), 1u) << "iter=" << iter << " bit=" << b;
        EXPECT_EQ(hlv_d.bit_count("X", "0", b), 0u) << "iter=" << iter << " bit=" << b;
      }

      if (iter == 14) {
        const std::string xml_p = "sysc_toggle_t0_overwrite.xml";
        xml_printer::coverage_save(xml_p, cntxt);
        expect_valid_ucis_schema(xml_p);
      }
    }
    fc4sc::global::delete_context(cntxt);
  }
}

namespace {

struct expanded_toggle_dut : public sc_core::sc_module {
  sc_core::sc_in<bool> in_b{"in_b"};
  sc_core::sc_out_resolved out_res{"out_res"};
  sc_core::sc_out_rv<4> out_rv{"out_rv"};
  sc_core::sc_export<sc_core::sc_signal_in_if<sc_dt::sc_uint<8>>> exp_in_u8{"exp_in_u8"};
  sc_core::sc_export<sc_core::sc_signal_inout_if<bool>> exp_io_b{"exp_io_b"};
  sc_core::sc_signal<sc_dt::sc_uint<8>> exp_u8_src{"exp_u8_src"};
  sc_core::sc_signal<bool> exp_b_src{"exp_b_src"};
  sc_core::sc_signal<long> sig_long{"sig_long"};
  sc_core::sc_signal<unsigned long> sig_ulong{"sig_ulong"};
  sc_core::sc_signal<sc_dt::sc_int<8>> sig_i8{"sig_i8"};
  sc_core::sc_signal<sc_dt::sc_bv<16>> sig_bv16{"sig_bv16"};
  sc_core::sc_signal_rv<4> sig_rv4{"sig_rv4"};
  sc_core::sc_signal<sc_dt::sc_bigint<72>> sig_bi72{"sig_bi72"};

  SC_CTOR(expanded_toggle_dut) {
    exp_in_u8(exp_u8_src);
    exp_io_b(exp_b_src);
  }
};

struct sos_toggle_mod : public sc_core::sc_module {
  sc_core::sc_signal<bool> s1{"s1"};
  sc_core::sc_signal<bool> s2{"s2"};
  fc4sc::global* ctx = nullptr;
  fc4sc::toggle_handle h1;
  fc4sc::toggle_handle h2;

  SC_CTOR(sos_toggle_mod) {
    SC_METHOD(t0);
  }

  void start_of_simulation() override {
    fc4sc::toggle_options to;
    to.cntxt = ctx;
    to.sample_mode = fc4sc::toggle_sample_mode::delta;
    h1 = fc4sc::track_toggle(s1, to);
    h2 = fc4sc::track_toggle(s2, to);
  }

  void t0() {
    s1.write(false);
    s2.write(false);
  }
};

struct eoe_toggle_mod : public sc_core::sc_module {
  sc_core::sc_signal<bool> s{"s"};
  fc4sc::global* ctx = nullptr;
  fc4sc::toggle_handle h;

  SC_CTOR(eoe_toggle_mod) {
    SC_METHOD(t0);
  }

  void end_of_elaboration() override {
    fc4sc::toggle_options to;
    to.cntxt = ctx;
    to.sample_mode = fc4sc::toggle_sample_mode::delta;
    h = fc4sc::track_toggle(s, to);
  }

  void t0() {
    s.write(false);
  }
};

} // namespace

TEST_F(SystemCToggleTest, track_module_toggles_vectors_exports_and_extra_types) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sc_core::sc_signal<bool> b_sig{"b_sig"};
    sc_core::sc_signal_resolved res_sig{"res_sig"};
    sc_core::sc_signal_rv<4> rv_sig{"rv_sig"};
    expanded_toggle_dut dut("exp_dut");
    dut.in_b(b_sig);
    dut.out_res(res_sig);
    dut.out_rv(rv_sig);

    fc4sc::toggle_options opts;
    opts.cntxt = cntxt;
    auto handles = fc4sc::track_module_toggles<sc_dt::sc_bigint<72>>(dut, opts);

    bool found_out_res = false;
    bool found_out_rv = false;
    bool found_exp_u8 = false;
    bool found_exp_b = false;
    bool found_bi72 = false;
    bool found_bv16 = false;
    bool found_long = false;
    size_t bi72_matches = 0;

    for (size_t i = 0; i < handles.size(); ++i) {
      const auto d = handles[i].data();
      if (!d) continue;
      if (d->name == "out_res") {
        found_out_res = true;
        EXPECT_EQ(d->port_direction, "out");
      } else if (d->name == "out_rv") {
        found_out_rv = true;
        EXPECT_EQ(d->port_direction, "out");
      } else if (d->name == "exp_in_u8") {
        found_exp_u8 = true;
      } else if (d->name == "exp_io_b") {
        found_exp_b = true;
      } else if (d->name == "sig_bi72") {
        found_bi72 = true;
        ++bi72_matches;
      } else if (d->name == "sig_bv16") {
        found_bv16 = true;
      } else if (d->name == "sig_long") {
        found_long = true;
      }
    }

    EXPECT_TRUE(found_out_res);
    EXPECT_TRUE(found_out_rv);
    EXPECT_TRUE(found_exp_u8);
    EXPECT_TRUE(found_exp_b);
    EXPECT_TRUE(found_bi72);
    EXPECT_EQ(bi72_matches, 1u);
    EXPECT_TRUE(found_bv16);
    EXPECT_TRUE(found_long);

    // Ensure duplicate ExtraTypes does not double-register
    auto handles_dup = fc4sc::track_module_toggles<sc_dt::sc_uint<8>, sc_dt::sc_bigint<72>>(dut, opts);
    EXPECT_EQ(handles_dup.size(), handles.size());

    dut.exp_u8_src.write(sc_dt::sc_uint<8>(0));
    dut.exp_b_src.write(false);
    dut.sig_long.write(0L);
    dut.sig_ulong.write(0UL);
    dut.sig_i8.write(sc_dt::sc_int<8>(0));
    dut.sig_bv16.write(sc_dt::sc_bv<16>(0));
    dut.sig_rv4.write(sc_dt::sc_lv<4>("0000"));
    dut.sig_bi72.write(sc_dt::sc_bigint<72>(0));
    b_sig.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);

    dut.exp_u8_src.write(sc_dt::sc_uint<8>(0x05));
    dut.exp_b_src.write(true);
    dut.sig_long.write(1L);
    dut.sig_ulong.write(2UL);
    dut.sig_i8.write(sc_dt::sc_int<8>(-1));
    dut.sig_bv16.write(sc_dt::sc_bv<16>(0x8001));
    dut.sig_rv4.write(sc_dt::sc_lv<4>("0101"));
    dut.sig_bi72.write(sc_dt::sc_bigint<72>(1) << 70);
    dut.out_res.write(sc_dt::SC_LOGIC_1);
    dut.out_rv.write(sc_dt::sc_lv<4>("1010"));
    sc_core::sc_start(2, sc_core::SC_NS);
    sc_core::sc_stop();

    for (size_t i = 0; i < handles.size(); ++i) {
      const auto d = handles[i].data();
      if (d && d->name == "sig_bi72") {
        EXPECT_EQ(handles[i].bit_count("0", "1", 70), 1u);
      } else if (d && d->name == "exp_in_u8") {
        EXPECT_EQ(handles[i].bit_count("0", "1", 0), 1u);
        EXPECT_EQ(handles[i].bit_count("0", "1", 2), 1u);
      } else if (d && d->name == "exp_io_b") {
        EXPECT_EQ(handles[i].bit_count("0", "1", 0), 1u);
      } else if (d && d->name == "sig_bv16") {
        EXPECT_EQ(handles[i].bit_count("0", "1", 15), 1u);
      }
    }
  }
  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCToggleTest, start_of_simulation_and_end_of_elaboration_paused_reset_aba_and_flush) {
  // 1. Pre-sc_start() sc_flush_toggles() must NOT mark baseline initialized prematurely,
  // and SC_ZERO_TIME + sc_flush_toggles() + subsequent sc_start() must not double-count.
  {
    auto cntxt = fc4sc::global::create_new_context();
    sc_core::sc_signal<bool> s{"s"};
    fc4sc::toggle_options ts_opts;
    ts_opts.cntxt = cntxt;
    ts_opts.sample_mode = fc4sc::toggle_sample_mode::timestep;
    auto h = fc4sc::track_toggle(s, ts_opts);
    s.write(true);
    fc4sc::sc_flush_toggles();
    EXPECT_EQ(h.bit_count("0", "1", 0), 0u);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(h.bit_count("0", "1", 0), 0u);
    s.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    fc4sc::sc_flush_toggles();
    EXPECT_EQ(h.bit_count("1", "0", 0), 1u);
    sc_core::sc_start(5, sc_core::SC_NS);
    EXPECT_EQ(h.bit_count("1", "0", 0), 1u);
    fc4sc::global::delete_context(cntxt);
  }

  // 2. 30x paused-reset ABA stress with start_of_simulation() registration (no sc_stop())
  const int warn_before = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);
  for (int iter = 0; iter < 30; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      sos_toggle_mod m1("m1");
      sos_toggle_mod m2("m2");
      m1.ctx = cntxt;
      m2.ctx = cntxt;
      m1.s1.write(true);
      m1.s2.write(true);
      m2.s1.write(true);
      m2.s2.write(true);
      sc_core::sc_start(4, sc_core::SC_NS);
      EXPECT_EQ(m1.h1.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(m1.h2.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h1.bit_count("1", "0", 0), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h2.bit_count("1", "0", 0), 1u) << "iter=" << iter;
    }
    fc4sc::global::delete_context(cntxt);
  }

  // 3. 30x paused-reset ABA stress with end_of_elaboration() registration (no sc_stop())
  for (int iter = 0; iter < 30; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      eoe_toggle_mod m("eoe_m");
      m.ctx = cntxt;
      m.s.write(true);
      sc_core::sc_start(4, sc_core::SC_NS);
      EXPECT_EQ(m.h.bit_count("1", "0", 0), 1u) << "eoe iter=" << iter;
    }
    fc4sc::global::delete_context(cntxt);
  }
  EXPECT_EQ(sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) - warn_before, 0);
}
