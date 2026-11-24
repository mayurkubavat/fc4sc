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

#include <cstdlib>
#include <fstream>
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

class SystemCSvaTest : public ::testing::Test {
protected:
  void SetUp() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
  void TearDown() override {
    sc_core::sc_get_curr_simcontext()->reset();
  }
};

struct rtl_pipe_mod : public sc_core::sc_module {
  sc_core::sc_in<bool> clk{"clk"};
  sc_core::sc_signal<bool> req{"req"};
  sc_core::sc_signal<bool> ack{"ack"};
  sc_core::sc_signal<bool> rst{"rst"};
  sc_core::sc_signal<int> data{"data"};

  SC_CTOR(rtl_pipe_mod) {
    SC_METHOD(pipe_proc);
    sensitive << clk.pos();
    dont_initialize();
  }

  void pipe_proc() {
    if (rst.read()) {
      ack.write(false);
    } else {
      ack.write(req.read());
    }
  }
};

} // namespace

TEST_F(SystemCSvaTest, preponed_sampling_on_registered_rtl_pipeline) {
  auto cntxt = fc4sc::global::create_new_context();
  sc_core::sc_clock clk("clk", 10, sc_core::SC_NS, 0.5, 5, sc_core::SC_NS, true);
  rtl_pipe_mod dut("dut");
  dut.clk(clk);

  fc4sc::sva_options opts;
  opts.cntxt = cntxt;
  opts.no_throw = true;

  // Same-cycle |-> must FAIL at the posedge where req=1 and registered ack is still 0 in Preponed!
  auto a_same = FC4SC_ASSERT_PROPERTY(
      "req_same_cycle_ack",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(fc4sc::sva::val(dut.req), fc4sc::sva::val(dut.ack)),
      opts);

  // Next-cycle |=> must PASS (1 pass, 0 fails, vacuous when req=0)!
  auto a_next = FC4SC_ASSERT_PROPERTY(
      "req_next_cycle_ack",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::next_implies(fc4sc::sva::val(dut.req), fc4sc::sva::val(dut.ack))
          .disable_iff(fc4sc::sva::val(dut.rst)),
      opts);

  auto c_req_ack = FC4SC_COVER_PROPERTY(
      "cov_req_then_ack",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(dut.req).then(1, fc4sc::sva::val(dut.ack)),
      opts);

  const int err_before = sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);

  dut.rst.write(false);
  dut.req.write(false);
  dut.ack.write(false);
  sc_core::sc_start(1, sc_core::SC_NS); // t=1ns (before first posedge at 5ns)

  dut.req.write(true);
  sc_core::sc_start(10, sc_core::SC_NS); // t=11ns (posedge at 5ns saw req=1, ack=0; ack became 1 at 5ns)
  dut.req.write(false);
  sc_core::sc_start(10, sc_core::SC_NS); // t=21ns (posedge at 15ns saw req=0, ack=1)
  sc_core::sc_stop();
  fc4sc::sc_flush_sva();

  EXPECT_EQ(a_same.fail_count(), 1u);
  EXPECT_EQ(a_same.pass_count(), 0u);
  EXPECT_EQ(a_same.vacuous_count(), 1u);
  EXPECT_EQ(a_next.pass_count(), 1u);
  EXPECT_EQ(a_next.fail_count(), 0u);
  EXPECT_EQ(a_next.vacuous_count(), 1u);
  EXPECT_EQ(c_req_ack.cover_count(), 1u);
  EXPECT_EQ(sc_core::sc_report_handler::get_count(sc_core::SC_ERROR) - err_before, 1);

  const std::string xml_path = "sysc_sva_out.xml";
  xml_printer::coverage_save(xml_path, cntxt);
  expect_valid_ucis_schema(xml_path);

  const std::string merged_path = "sysc_sva_merged.xml";
  fc4sc::merge_coverage_db({xml_path, xml_path}, merged_path);
  expect_valid_ucis_schema(merged_path);

  const std::string md = fc4sc::generate_markdown_report(cntxt);
  const std::string html = fc4sc::generate_html_report(cntxt);
  EXPECT_NE(md.find("Assertion Coverage"), std::string::npos);
  EXPECT_NE(html.find("Assertion Coverage"), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCSvaTest, temporal_operators_sampled_funcs_disable_iff_and_glitch_clk) {
  auto cntxt = fc4sc::global::create_new_context();
  sc_core::sc_signal<bool> clk("clk");
  sc_core::sc_signal<bool> a("a");
  sc_core::sc_signal<bool> b("b");
  sc_core::sc_signal<bool> c("c");
  sc_core::sc_signal<bool> rst("rst");
  sc_core::sc_signal<int> cnt("cnt");

  fc4sc::sva_options opts;
  opts.cntxt = cntxt;
  opts.no_throw = true;

  // Bounded delay ##[1:2], unbounded ##[1:$], repetition [*2] & [*1:2], ##0, $rose/$fell/$past
  auto a_range = FC4SC_ASSERT_PROPERTY(
      "range_delay",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          FC4SC_ROSE(a),
          fc4sc::sva::delay(1, 2, fc4sc::sva::val(b)))
          .disable_iff(fc4sc::sva::val(rst)),
      opts);

  auto a_unbounded = FC4SC_ASSERT_PROPERTY(
      "unbounded_delay",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          fc4sc::sva::val(a),
          fc4sc::sva::delay_unbounded(1, fc4sc::sva::val(c))),
      opts);

  auto a_rep = FC4SC_ASSERT_PROPERTY(
      "rep_and_past",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          fc4sc::sva::val(b).repeat(2),
          fc4sc::sva::val(cnt) == FC4SC_PAST(cnt, 2)),
      opts);

  auto a_fusion = FC4SC_ASSERT_PROPERTY(
      "fusion_and_fell",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          fc4sc::sva::val(a).then(0, FC4SC_FELL(b)),
          fc4sc::sva::val(c)),
      opts);

  // Multi-match antecedent universal quantification: (a ##[1:2] b) |-> c
  // If b matches at tick 1 AND tick 2, BOTH tick 1 and tick 2 must satisfy c!
  auto a_multimatch = FC4SC_ASSERT_PROPERTY(
      "multimatch_univ",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          fc4sc::sva::val(a).then(1, 2, fc4sc::sva::val(b)),
          fc4sc::sva::val(c)),
      opts);

  clk.write(false); a.write(false); b.write(false); c.write(false); rst.write(false); cnt.write(0);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Inject glitchy 0 -> 1 -> 0 clock pulse in deltas at t=1ns (must NOT trigger any attempt)
  clk.write(true);
  sc_core::sc_start(sc_core::SC_ZERO_TIME);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);
  EXPECT_EQ(a_range.attempt_count(), 0u);

  // Tick 0 (t=2ns): drive a=1, cnt=7 at t=1ns, then posedge clk at t=2ns
  a.write(true); cnt.write(7);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 1 (t=6ns): drive b=1, c=1, cnt=9 at t=5ns, then posedge clk at t=6ns
  b.write(true); c.write(true); cnt.write(9);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 2 (t=10ns): drive a=0, cnt=7 at t=9ns, then posedge clk at t=10ns
  a.write(false); cnt.write(7);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 3 (t=14ns): drive a=1, b=0 (fell) at t=13ns, then posedge clk at t=14ns
  a.write(true); b.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Async reset pulse between ticks disables the active in-flight attempt from tick 3!
  rst.write(true);
  sc_core::sc_start(1, sc_core::SC_NS);
  rst.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);
  sc_core::sc_stop();
  fc4sc::sc_flush_sva();

  EXPECT_EQ(a_range.pass_count(), 1u);
  EXPECT_EQ(a_range.disabled_count(), 1u);
  EXPECT_GE(a_range.peak_active_count(), 1u);
  EXPECT_EQ(a_unbounded.pass_count(), 2u);
  EXPECT_EQ(a_rep.pass_count(), 1u);
  EXPECT_EQ(a_fusion.pass_count(), 1u);
  EXPECT_GE(a_multimatch.pass_count(), 1u);
  EXPECT_EQ(a_multimatch.fail_count(), 0u);

  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCSvaTest, batch_exception_isolation_and_reset_aba) {
  {
    auto cntxt = fc4sc::global::create_new_context();
    sc_core::sc_signal<bool> clk("clk");
    sc_core::sc_signal<bool> sig("sig");

    // Default throwing SC_REPORT_ERROR mode
    fc4sc::sva_options throw_opts;
    throw_opts.cntxt = cntxt;
    throw_opts.no_throw = false;

    auto a1 = FC4SC_ASSERT_PROPERTY("fail_first", fc4sc::sva::posedge(clk), fc4sc::sva::val(sig), throw_opts);
    auto a2 = FC4SC_ASSERT_PROPERTY("fail_second", fc4sc::sva::posedge(clk), fc4sc::sva::val(sig), throw_opts);

    clk.write(false); sig.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);
    clk.write(true);
    bool caught = false;
    try {
      sc_core::sc_start(2, sc_core::SC_NS);
    } catch (const sc_core::sc_report&) {
      caught = true;
    }
    EXPECT_TRUE(caught);
    // Batch isolation: BOTH assertions updated their UCIS bins before exception unwind!
    EXPECT_EQ(a1.fail_count(), 1u);
    EXPECT_EQ(a2.fail_count(), 1u);
    fc4sc::global::delete_context(cntxt);
  }

  // 10x sc_simcontext::reset() ABA test
  for (int iter = 0; iter < 10; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto c = fc4sc::global::create_new_context();
    {
      sc_core::sc_signal<bool> rclk{"rclk"};
      sc_core::sc_signal<bool> rsig{"rsig"};
      fc4sc::sva_options o;
      o.cntxt = c;
      o.no_throw = true;
      auto ah = FC4SC_ASSERT_PROPERTY("aba_p", fc4sc::sva::posedge(rclk), fc4sc::sva::val(rsig), o);
      rclk.write(false); rsig.write(true);
      sc_core::sc_start(1, sc_core::SC_NS);
      rclk.write(true);
      sc_core::sc_start(2, sc_core::SC_NS);
      sc_core::sc_stop();
      EXPECT_EQ(ah.pass_count(), 1u);
    }
    fc4sc::global::delete_context(c);
  }
}

TEST_F(SystemCSvaTest, or_overload_zero_repeat_disable_iff_sampled_and_compound_expr) {
  auto cntxt = fc4sc::global::create_new_context();
  sc_core::sc_signal<bool> clk("clk");
  sc_core::sc_signal<bool> a("a");
  sc_core::sc_signal<bool> b("b");
  sc_core::sc_signal<bool> rst1("rst1");
  sc_core::sc_signal<bool> rst2("rst2");
  sc_core::sc_signal<sc_dt::sc_logic> rst_logic("rst_logic");
  sc_core::sc_signal<int> x("x");
  sc_core::sc_signal<int> y("y");

  fc4sc::sva_options opts;
  opts.cntxt = cntxt;
  opts.no_throw = true;

  // Bug 2: operator|| on expr<bool> || expr<bool> in sequence and disable_iff
  auto c_or = FC4SC_COVER_PROPERTY(
      "cov_or_expr_bool",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a) || fc4sc::sva::val(b),
      opts);

  auto a_or_dis = FC4SC_ASSERT_PROPERTY(
      "assert_or_disable",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(fc4sc::sva::val(a), fc4sc::sva::val(b))
          .disable_iff(fc4sc::sva::val(rst1) || fc4sc::sva::val(rst2)),
      opts);

  // Bug 3: IEEE 1800-2023 §16.9.2 zero-repeat concatenation semantics
  auto c_z02_2 = FC4SC_COVER_PROPERTY(
      "z02_then2",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a).repeat(0, 2).then(2, fc4sc::sva::val(b)),
      opts);

  auto c_z0_0 = FC4SC_COVER_PROPERTY(
      "z0_then0",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a).repeat(0).then(0, fc4sc::sva::val(b)),
      opts);

  auto c_z0_1 = FC4SC_COVER_PROPERTY(
      "z0_then1",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a).repeat(0).then(1, fc4sc::sva::val(b)),
      opts);

  auto c_rhs_z0_0 = FC4SC_COVER_PROPERTY(
      "rhs_z0_then0",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a).then(0, fc4sc::sva::val(b).repeat(0)),
      opts);

  auto c_rhs_z0_1 = FC4SC_COVER_PROPERTY(
      "rhs_z0_then1",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::val(a).then(1, fc4sc::sva::val(b).repeat(0)),
      opts);

  // Bug 4: FC4SC_ROSE / FC4SC_FELL / FC4SC_STABLE / FC4SC_CHANGED inside disable_iff
  auto a_dis_rose = FC4SC_ASSERT_PROPERTY(
      "dis_rose_logic",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(fc4sc::sva::val(a), fc4sc::sva::val(b))
          .disable_iff(FC4SC_ROSE(rst_logic)),
      opts);

  auto a_dis_changed = FC4SC_ASSERT_PROPERTY(
      "dis_changed_logic",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(fc4sc::sva::val(a), fc4sc::sva::val(b))
          .disable_iff(FC4SC_CHANGED(rst_logic)),
      opts);

  // Reviewer 2 Minor: compound expr<T> in FC4SC_PAST / FC4SC_ROSE / FC4SC_CHANGED
  auto a_compound_past = FC4SC_ASSERT_PROPERTY(
      "compound_past",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(
          FC4SC_ROSE(fc4sc::sva::val(x) + fc4sc::sva::val(y)),
          FC4SC_PAST(fc4sc::sva::val(x) + fc4sc::sva::val(y), 1) == 0),
      opts);

  clk.write(false);
  a.write(false);
  b.write(true);
  rst1.write(false);
  rst2.write(true); // rst1 || rst2 == true at Tick 0
  rst_logic.write(sc_dt::SC_LOGIC_1);
  x.write(0);
  y.write(1);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 0: a=0, b=1, rst2=1, rst_logic=1 (X->1 rose + changed), x+y=1 (0->1 rose)
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  EXPECT_EQ(c_or.cover_count(), 1u);
  EXPECT_EQ(a_or_dis.disabled_count(), 1u);
  EXPECT_EQ(c_z02_2.cover_count(), 0u); // must NOT match at Tick 0 (needs ##1 b)
  EXPECT_EQ(c_z0_0.cover_count(), 0u);  // empty ##0 b never matches
  EXPECT_EQ(c_z0_1.cover_count(), 1u);  // empty ##1 b == b at Tick 0
  EXPECT_EQ(c_rhs_z0_0.cover_count(), 0u);
  EXPECT_EQ(a_dis_rose.disabled_count(), 1u);
  EXPECT_EQ(a_dis_changed.disabled_count(), 1u);
  EXPECT_EQ(a_compound_past.pass_count(), 1u);

  // Tick 1: a=1, b=0, rst2=0, rst_logic stays 1 (NOT rose, NOT changed!), x+y=2
  a.write(true);
  b.write(false);
  rst2.write(false);
  x.write(1);
  y.write(1);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // c_z02_2: attempt 0 sees b=0 at Tick 1 (still 0); a_dis_rose/changed: a=1,b=0 -> FAIL!
  EXPECT_EQ(c_z02_2.cover_count(), 0u);
  EXPECT_EQ(c_rhs_z0_1.cover_count(), 1u); // a=1 at Tick 1 -> a ##1 b[*0] == a
  EXPECT_EQ(a_dis_rose.disabled_count(), 1u);
  EXPECT_EQ(a_dis_rose.fail_count(), 1u);
  EXPECT_EQ(a_dis_changed.disabled_count(), 1u);
  EXPECT_EQ(a_dis_changed.fail_count(), 1u);
  EXPECT_EQ(a_or_dis.fail_count(), 1u);

  // Tick 2: a=0, b=1 -> c_z02_2 attempt from Tick 1 (b=1 at Tick 2 via ##1 b) matches!
  a.write(false);
  b.write(true);
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  sc_core::sc_stop();
  fc4sc::sc_flush_sva();

  EXPECT_EQ(c_z02_2.cover_count(), 1u);
  EXPECT_EQ(c_z0_0.cover_count(), 0u);
  EXPECT_EQ(c_rhs_z0_0.cover_count(), 0u);

  const std::string xml_path = "sysc_sva_reg.xml";
  xml_printer::coverage_save(xml_path, cntxt);
  expect_valid_ucis_schema(xml_path);

  fc4sc::global::delete_context(cntxt);
}

namespace {

SC_MODULE(sva_t0_write_mod) {
  sc_core::sc_signal<int>* x_ptr = nullptr;
  SC_CTOR(sva_t0_write_mod) { SC_METHOD(write_t0); }
  void write_t0() {
    if (x_ptr) x_ptr->write(99);
  }
};

} // namespace

TEST_F(SystemCSvaTest, pre_sc_start_clock_and_data_init_and_aba_reset) {
  // 15x reset() ABA testing pre-sc_start() clk.write(true), sc_clock(..., 0, SC_NS, true),
  // and t=0 delta-0 SC_METHOD write (must not clobber Preponed t=0 sample)
  for (int iter = 0; iter < 15; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      sc_core::sc_signal<bool> clk_hi{"clk_hi"};
      sc_core::sc_signal<int> x_hi{"x_hi"};
      sc_core::sc_clock clk_t0("clk_t0", 10, sc_core::SC_NS, 0.5, 0, sc_core::SC_NS, true);
      sc_core::sc_signal<int> x_t0{"x_t0"};
      sc_core::sc_signal<int> x_over{"x_over"};
      sva_t0_write_mod t0_mod("t0_mod");
      t0_mod.x_ptr = &x_over;

      fc4sc::sva_options opts;
      opts.cntxt = cntxt;
      opts.no_throw = true;

      auto a_hi = FC4SC_ASSERT_PROPERTY(
          "clk_pre_hi",
          fc4sc::sva::posedge(clk_hi),
          fc4sc::sva::val(x_hi) == 42,
          opts);
      auto a_t0 = FC4SC_ASSERT_PROPERTY(
          "clk_t0_edge",
          fc4sc::sva::posedge(clk_t0),
          fc4sc::sva::val(x_t0) == 42,
          opts);
      auto a_t0_over = FC4SC_ASSERT_PROPERTY(
          "clk_t0_over",
          fc4sc::sva::posedge(clk_t0),
          fc4sc::sva::val(x_over) == 42,
          opts);

      clk_hi.write(true);
      x_hi.write(42);
      x_t0.write(42);
      x_over.write(42);

      sc_core::sc_start(4, sc_core::SC_NS);
      sc_core::sc_stop();

      EXPECT_EQ(a_hi.attempt_count(), 0u) << "iter=" << iter;
      EXPECT_EQ(a_hi.pass_count(), 0u) << "iter=" << iter;
      EXPECT_EQ(a_hi.fail_count(), 0u) << "iter=" << iter;

      EXPECT_EQ(a_t0.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(a_t0.pass_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(a_t0.fail_count(), 0u) << "iter=" << iter;

      EXPECT_EQ(a_t0_over.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(a_t0_over.pass_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(a_t0_over.fail_count(), 0u) << "iter=" << iter;

      if (iter == 14) {
        const std::string xml_p = "sysc_sva_preinit.xml";
        xml_printer::coverage_save(xml_p, cntxt);
        expect_valid_ucis_schema(xml_p);
      }
    }
    fc4sc::global::delete_context(cntxt);
  }
}

namespace {

struct sva_export_mod : public sc_core::sc_module {
  sc_core::sc_export<sc_core::sc_signal_in_if<bool>> clk_exp{"clk_exp"};
  sc_core::sc_export<sc_core::sc_signal_inout_if<sc_dt::sc_uint<8>>> data_exp{"data_exp"};
  sc_core::sc_signal<sc_dt::sc_uint<8>> data_sig{"data_sig"};

  explicit sva_export_mod(sc_core::sc_module_name nm, const sc_core::sc_signal_in_if<bool>& clk_if)
      : sc_core::sc_module(nm) {
    clk_exp.bind(const_cast<sc_core::sc_signal_in_if<bool>&>(clk_if));
    data_exp.bind(data_sig);
  }
};

struct sos_sva_mod : public sc_core::sc_module {
  sc_core::sc_clock clk{"clk", 10, sc_core::SC_NS, 0.5, 0, sc_core::SC_NS, true};
  sc_core::sc_signal<int> x{"x"};
  fc4sc::global* ctx = nullptr;
  fc4sc::sva_handle h1;
  fc4sc::sva_handle h2;

  SC_CTOR(sos_sva_mod) {
    SC_METHOD(t0);
  }

  void start_of_simulation() override {
    fc4sc::sva_options so;
    so.cntxt = ctx;
    so.no_throw = true;
    h1 = FC4SC_ASSERT_PROPERTY("sos_aba_1", fc4sc::sva::posedge(clk), fc4sc::sva::val(x) == 42, so);
    h2 = FC4SC_ASSERT_PROPERTY("sos_aba_2", fc4sc::sva::posedge(clk), fc4sc::sva::val(x) == 42, so);
  }

  void t0() {
    x.write(99);
  }
};

struct eoe_sva_mod : public sc_core::sc_module {
  sc_core::sc_clock clk{"clk", 10, sc_core::SC_NS, 0.5, 0, sc_core::SC_NS, true};
  sc_core::sc_signal<int> x{"x"};
  fc4sc::global* ctx = nullptr;
  fc4sc::sva_handle h;

  SC_CTOR(eoe_sva_mod) {
    SC_METHOD(t0);
  }

  void end_of_elaboration() override {
    fc4sc::sva_options so;
    so.cntxt = ctx;
    so.no_throw = true;
    h = FC4SC_ASSERT_PROPERTY("eoe_aba", fc4sc::sva::posedge(clk), fc4sc::sva::val(x) == 42, so);
  }

  void t0() {
    x.write(99);
  }
};

} // namespace

TEST_F(SystemCSvaTest, systemc_datatypes_lsb_pred_and_ports) {
  auto cntxt = fc4sc::global::create_new_context();
  sc_core::sc_signal<bool> clk{"clk"};
  sc_core::sc_signal<sc_dt::sc_uint<8>> u8{"u8"};
  sc_core::sc_signal<sc_dt::sc_int<8>> i8{"i8"};
  sc_core::sc_signal<sc_dt::sc_int<8>> i8_2{"i8_2"};
  sc_core::sc_signal<sc_dt::sc_biguint<70>> bu70{"bu70"};
  sc_core::sc_signal<sc_dt::sc_bigint<70>> bi70{"bi70"};
  sc_core::sc_signal<sc_dt::sc_bv<4>> bv4{"bv4"};
  sc_core::sc_signal<sc_dt::sc_lv<4>> lv4{"lv4"};
  sc_core::sc_signal<sc_dt::sc_logic> log_sig{"log_sig"};
  sc_core::sc_signal_resolved res_sig{"res_sig"};
  sc_core::sc_signal_rv<4> rv_sig{"rv_sig"};
  sva_export_mod exp_mod("exp_mod", clk);

  fc4sc::sva_options opts;
  opts.cntxt = cntxt;
  opts.no_throw = true;

  auto c_u8 = FC4SC_COVER_PROPERTY("rose_u8", fc4sc::sva::posedge(clk), FC4SC_ROSE(u8), opts);
  auto c_i8 = FC4SC_COVER_PROPERTY("fell_i8", fc4sc::sva::posedge(clk), FC4SC_FELL(i8), opts);
  auto c_bu70 = FC4SC_COVER_PROPERTY("rose_bu70", fc4sc::sva::posedge(clk), FC4SC_ROSE(bu70), opts);
  auto c_bi70_expr = FC4SC_COVER_PROPERTY("fell_bi70_expr", fc4sc::sva::posedge(clk), FC4SC_FELL(fc4sc::sva::val(bi70)), opts);
  auto c_sum_expr = FC4SC_COVER_PROPERTY("rose_sum_expr", fc4sc::sva::posedge(clk), FC4SC_ROSE(fc4sc::sva::val(i8) + fc4sc::sva::val(i8_2)), opts);
  auto a_bi70_clk = FC4SC_ASSERT_PROPERTY("clk_bi70", fc4sc::sva::posedge(bi70), fc4sc::sva::val(u8) == sc_dt::sc_uint<8>(2), opts);
  auto a_exp = FC4SC_ASSERT_PROPERTY("exp_clk_data", fc4sc::sva::posedge(exp_mod.clk_exp), fc4sc::sva::val(exp_mod.data_exp) == sc_dt::sc_uint<8>(5), opts);

  // Direct expr<T> -> pred_expr conversions (including >64-bit, 4-state, sc_logic without SC_WARNING)
  auto a_pred_log = FC4SC_ASSERT_PROPERTY("pred_logic", fc4sc::sva::posedge(clk), fc4sc::sva::val(log_sig), opts);
  auto a_pred_bv = FC4SC_ASSERT_PROPERTY("pred_bv", fc4sc::sva::posedge(clk), fc4sc::sva::val(bv4), opts);
  auto a_pred_lv = FC4SC_ASSERT_PROPERTY("pred_lv", fc4sc::sva::posedge(clk), fc4sc::sva::val(lv4), opts);
  auto a_pred_bi70 = FC4SC_ASSERT_PROPERTY("pred_bi70_hi", fc4sc::sva::posedge(clk), fc4sc::sva::val(bi70), opts);
  auto a_pred_bu70 = FC4SC_ASSERT_PROPERTY(
      "pred_bu70_dis_res",
      fc4sc::sva::posedge(clk),
      fc4sc::sva::implies(fc4sc::sva::val(bu70), fc4sc::sva::val(rv_sig))
          .disable_iff(fc4sc::sva::val(res_sig)),
      opts);

  const int warn_before = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);

  clk.write(false);
  u8.write(0);
  i8.write(-1); // bit 0 is 1
  i8_2.write(1);
  bu70.write(0);
  bi70.write((sc_dt::sc_bigint<70>(1) << 69) | sc_dt::sc_bigint<70>(1)); // bit 0 is 1
  bv4.write(sc_dt::sc_bv<4>("0000"));
  lv4.write(sc_dt::sc_lv<4>("00X0")); // or_reduce == Log_X -> false
  log_sig.write(sc_dt::SC_LOGIC_X); // == SC_LOGIC_1 -> false, zero SC_WARNING
  res_sig.write(sc_dt::SC_LOGIC_0);
  rv_sig.write(sc_dt::sc_lv<4>("0000"));
  exp_mod.data_sig.write(5);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 0: u8=1 (rose), i8=-1 (bit0=1), i8+i8_2=0 (bit0=0), bu70=1 (rose),
  // bi70=(1<<69)|1 (bit0=1, !=0), log_sig='1', bv4="0010", lv4="01X0" (or_reduce==Log_1), rv_sig="0001"
  u8.write(1);
  bu70.write(sc_dt::sc_biguint<70>(1));
  bv4.write(sc_dt::sc_bv<4>("0010"));
  lv4.write(sc_dt::sc_lv<4>("01X0"));
  log_sig.write(sc_dt::SC_LOGIC_1);
  rv_sig.write(sc_dt::sc_lv<4>("0001"));
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Tick 1: i8=-2 (bit0: 1->0 fell!), i8_2=3 (-2+3=1, bit0: 0->1 rose!),
  // bi70=(1<<69) (bit0: 1->0 fell!, !=0), u8=2, bv4="0000", lv4="00X0" (false),
  // log_sig='Z' (false), bu70=(1<<69), res_sig='1' (disables)
  u8.write(2);
  i8.write(-2);
  i8_2.write(3);
  bu70.write(sc_dt::sc_biguint<70>(1) << 69);
  bi70.write(sc_dt::sc_bigint<70>(1) << 69);
  bv4.write(sc_dt::sc_bv<4>("0000"));
  lv4.write(sc_dt::sc_lv<4>("00X0"));
  log_sig.write(sc_dt::SC_LOGIC_Z);
  res_sig.write(sc_dt::sc_logic('1'));
  sc_core::sc_start(1, sc_core::SC_NS);
  clk.write(true);
  sc_core::sc_start(2, sc_core::SC_NS);
  clk.write(false);
  sc_core::sc_start(1, sc_core::SC_NS);

  // Now u8==2 is settled in Preponed; drive posedge(bi70)
  bi70.write((sc_dt::sc_bigint<70>(1) << 69) | sc_dt::sc_bigint<70>(1));
  sc_core::sc_start(2, sc_core::SC_NS);
  sc_core::sc_stop();
  fc4sc::sc_flush_sva();

  EXPECT_EQ(sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) - warn_before, 0);
  EXPECT_EQ(c_u8.cover_count(), 1u);
  EXPECT_EQ(c_i8.cover_count(), 1u);
  EXPECT_EQ(c_bu70.cover_count(), 1u);
  EXPECT_EQ(c_bi70_expr.cover_count(), 1u);
  EXPECT_EQ(c_sum_expr.cover_count(), 1u);
  EXPECT_EQ(a_bi70_clk.pass_count(), 1u);
  EXPECT_EQ(a_bi70_clk.fail_count(), 0u);
  EXPECT_EQ(a_exp.pass_count(), 2u);
  EXPECT_EQ(a_exp.fail_count(), 0u);
  EXPECT_EQ(a_pred_log.pass_count(), 1u);
  EXPECT_EQ(a_pred_log.fail_count(), 1u);
  EXPECT_EQ(a_pred_bv.pass_count(), 1u);
  EXPECT_EQ(a_pred_bv.fail_count(), 1u);
  EXPECT_EQ(a_pred_lv.pass_count(), 1u);
  EXPECT_EQ(a_pred_lv.fail_count(), 1u);
  EXPECT_EQ(a_pred_bi70.pass_count(), 2u);
  EXPECT_EQ(a_pred_bi70.fail_count(), 0u);
  EXPECT_EQ(a_pred_bu70.pass_count(), 1u);
  EXPECT_EQ(a_pred_bu70.disabled_count(), 1u);

  const std::string xml_p = "sysc_sva_datatypes.xml";
  xml_printer::coverage_save(xml_p, cntxt);
  expect_valid_ucis_schema(xml_p);

  fc4sc::global::delete_context(cntxt);
}

TEST_F(SystemCSvaTest, start_of_simulation_and_end_of_elaboration_paused_reset_aba_and_flush) {
  // 1. Pre-sc_start() sc_flush_sva() must NOT initialize before delta 0, and
  // SC_ZERO_TIME + sc_flush_sva() + subsequent sc_start() must not double-count.
  {
    auto cntxt = fc4sc::global::create_new_context();
    sc_core::sc_clock clk{"clk", 10, sc_core::SC_NS, 0.5, 0, sc_core::SC_NS, true};
    sc_core::sc_signal<int> x{"x"};
    fc4sc::sva_options so;
    so.cntxt = cntxt;
    so.no_throw = true;
    auto h = FC4SC_ASSERT_PROPERTY("flush_guard", fc4sc::sva::posedge(clk), fc4sc::sva::val(x) == 42, so);
    x.write(42);
    fc4sc::sc_flush_sva();
    EXPECT_EQ(h.attempt_count(), 0u);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    fc4sc::sc_flush_sva();
    EXPECT_EQ(h.attempt_count(), 1u);
    EXPECT_EQ(h.pass_count(), 1u);

    sc_core::sc_start(4, sc_core::SC_NS);
    EXPECT_EQ(h.attempt_count(), 1u);
    EXPECT_EQ(h.pass_count(), 1u);
    fc4sc::global::delete_context(cntxt);
  }

  // 2. 30x paused-reset ABA stress with start_of_simulation() registration (no sc_stop())
  const int warn_before = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);
  for (int iter = 0; iter < 30; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      sos_sva_mod m1("m1");
      sos_sva_mod m2("m2");
      m1.ctx = cntxt;
      m2.ctx = cntxt;
      m1.x.write(42);
      m2.x.write(42);
      sc_core::sc_start(4, sc_core::SC_NS);
      EXPECT_EQ(m1.h1.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m1.h1.pass_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m1.h1.fail_count(), 0u) << "iter=" << iter;
      EXPECT_EQ(m1.h2.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m1.h2.pass_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h1.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h1.pass_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h2.attempt_count(), 1u) << "iter=" << iter;
      EXPECT_EQ(m2.h2.pass_count(), 1u) << "iter=" << iter;
    }
    fc4sc::global::delete_context(cntxt);
  }

  // 3. 30x paused-reset ABA stress with end_of_elaboration() registration (no sc_stop())
  for (int iter = 0; iter < 30; ++iter) {
    sc_core::sc_get_curr_simcontext()->reset();
    auto cntxt = fc4sc::global::create_new_context();
    {
      eoe_sva_mod m("eoe_m");
      m.ctx = cntxt;
      m.x.write(42);
      sc_core::sc_start(4, sc_core::SC_NS);
      EXPECT_EQ(m.h.attempt_count(), 1u) << "eoe iter=" << iter;
      EXPECT_EQ(m.h.pass_count(), 1u) << "eoe iter=" << iter;
      EXPECT_EQ(m.h.fail_count(), 0u) << "eoe iter=" << iter;
    }
    fc4sc::global::delete_context(cntxt);
  }
  EXPECT_EQ(sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) - warn_before, 0);
}
