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

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

using namespace fc4sc;

namespace {

class xss_and_types_cg : public fc4sc::covergroup {
public:
  CG_CONS(xss_and_types_cg) {
    option().per_instance = true;
    option().comment = "bad <script>alert('xss')</script> & | `pipe`";
  };

  int val = 0;
  double dval = 0.0;
  int seq_v = 0;

  COVERPOINT(int, cvp_main, val) {
    bin<int>("low|<tag>", interval(0, 1)),
    bin<int>("high", 2),
    ignore_bin<int>("ign", 3),
    illegal_bin<int>("ill", 4),
    default_bin<int>("def")
  };

  COVERPOINT(double, cvp_dbl, dval) {
    bin<double>("nominal", tolerance(5.0, 0.5))
  };

  COVERPOINT(int, cvp_seq, seq_v) {
    transition_bin<int>("s01", trans<int>(0, 1))
  };

  COVERPOINT(int, cvp_empty, val) {
    bin<int>("none", 99).with([](int) { return false; })
  };

  cross<int, int> crs_ab = cross<int, int>(this, "crs_<ab>", &cvp_main, &cvp_seq);
};

class disabled_cg : public fc4sc::covergroup {
public:
  CG_CONS(disabled_cg) {};
  int x = 0;
  COVERPOINT(int, cvp_x, x) { bin<int>("b0", 0) };
};

class ordered_scope_a : public fc4sc::scope {
public:
  SCOPE_DECL(ordered_scope_a);

  class scoped_cg : public fc4sc::covergroup {
  public:
    CG_SCOPED_CONS(scoped_cg, ordered_scope_a) {
      option().per_instance = true;
      option().comment = "bad <script>alert('xss')</script> & | `pipe`";
    };
    int val = 0;
    double dval = 0.0;
    int seq_v = 0;

    COVERPOINT(int, cvp_main, val) {
      bin<int>("low|<tag>", interval(0, 1)),
      bin<int>("high", 2),
      ignore_bin<int>("ign", 3),
      illegal_bin<int>("ill", 4),
      default_bin<int>("def")
    };

    COVERPOINT(double, cvp_dbl, dval) {
      bin<double>("nominal", tolerance(5.0, 0.5))
    };

    COVERPOINT(int, cvp_seq, seq_v) {
      transition_bin<int>("s01", trans<int>(0, 1))
    };

    COVERPOINT(int, cvp_empty, val) {
      bin<int>("none", 99).with([](int) { return false; })
    };

    cross<int, int> crs_ab = cross<int, int>(this, "crs_<ab>", &cvp_main, &cvp_seq);
  };

  ordered_scope_a(const std::string& n, fc4sc::global* cntxt)
      : fc4sc::scope(n, __FILE__, __LINE__, __FILE__, __LINE__, cntxt),
        CG_SCOPED_INST(cg_z),
        CG_SCOPED_INST(cg_a) {}

  scoped_cg cg_z;
  scoped_cg cg_a;
};

} // namespace

TEST(report_generator, live_markdown_and_html_escaping_and_determinism) {
  auto c1 = fc4sc::global::create_new_context();
  auto c2 = fc4sc::global::create_new_context();

  ordered_scope_a s1("top_<script>", c1);
  ordered_scope_a s2("top_<script>", c2);

  s1.cg_a.val = 0; s1.cg_a.dval = 5.1; s1.cg_a.seq_v = 0; s1.cg_a.sample();
  s1.cg_a.seq_v = 1; s1.cg_a.sample();
  s2.cg_a.val = 0; s2.cg_a.dval = 5.1; s2.cg_a.seq_v = 0; s2.cg_a.sample();
  s2.cg_a.seq_v = 1; s2.cg_a.sample();

  fc4sc::dynamic_scope_factory dis_scp_fact("dis_scp_type");
  fc4sc::dynamic_covergroup_factory dis_cg_fact(dis_scp_fact, "dis_cg_type");
  auto dis_cvp = dis_cg_fact.create_coverpoint<int(int)>("x", [](int v) { return v; });
  dis_cvp.create_bin("b0", 0);
  dis_scp_fact.add_covergroup(dis_cg_fact, "dis_inst");

  fc4sc::dynamic_scope ds1(dis_scp_fact, "ds_1", __FILE__, __LINE__, c1);
  fc4sc::dynamic_scope ds2(dis_scp_fact, "ds_1", __FILE__, __LINE__, c2);
  ds1.get_covergroup("dis_inst").disable_cvg();
  ds2.get_covergroup("dis_inst").disable_cvg();

  fc4sc::report_options opts;
  opts.title = "Report <Title> & \"Quotes\"";

  const std::string md1 = fc4sc::generate_markdown_report(c1, opts);
  const std::string md2 = fc4sc::generate_markdown_report(c2, opts);
  const std::string html1 = fc4sc::generate_html_report(c1, opts);
  const std::string html2 = fc4sc::generate_html_report(c2, opts);

  EXPECT_EQ(md1, md2);
  EXPECT_EQ(html1, html2);

  // Self-contained HTML checks: inline CSS, zero <script> tags, zero external URLs
  EXPECT_NE(html1.find("<style>"), std::string::npos);
  EXPECT_EQ(html1.find("<script>"), std::string::npos);
  EXPECT_EQ(html1.find("http://"), std::string::npos);
  EXPECT_EQ(html1.find("https://"), std::string::npos);
  EXPECT_NE(html1.find("&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;"), std::string::npos);
  EXPECT_NE(html1.find("Report &lt;Title&gt; &amp; &quot;Quotes&quot;"), std::string::npos);

  // Markdown escaping checks
  EXPECT_NE(md1.find("&lt;script&gt;"), std::string::npos);
  EXPECT_NE(md1.find("low\\|&lt;tag&gt;"), std::string::npos);
  EXPECT_NE(md1.find("\\`pipe\\`"), std::string::npos);

  // Deterministic ordering: cg_a precedes cg_z
  const size_t pos_a = md1.find("cg_a");
  const size_t pos_z = md1.find("cg_z");
  ASSERT_NE(pos_a, std::string::npos);
  ASSERT_NE(pos_z, std::string::npos);
  EXPECT_LT(pos_a, pos_z);

  // Optional toggle/assertion/other-metric sections must be omitted when empty
  EXPECT_EQ(md1.find("Toggle Coverage"), std::string::npos);
  EXPECT_EQ(md1.find("Assertion Coverage"), std::string::npos);
  EXPECT_EQ(md1.find("Other UCIS Metrics"), std::string::npos);

  // Human-readable double tolerance values present
  EXPECT_NE(md1.find("4.5"), std::string::npos);
  EXPECT_NE(md1.find("5.5"), std::string::npos);

  // File save APIs
  fc4sc::save_markdown_report("report_test_out.md", c1, opts);
  fc4sc::save_html_report("report_test_out.html", c1, opts);
  std::ifstream md_in("report_test_out.md");
  std::ifstream html_in("report_test_out.html");
  EXPECT_TRUE(md_in.good());
  EXPECT_TRUE(html_in.good());

  fc4sc::global::delete_context(c1);
  fc4sc::global::delete_context(c2);
}

TEST(report_generator, xml_report_includes_toggle_assertion_other_metrics_and_lv_attrs) {
  const std::string xml_path = "report_7metric_input.xml";
  {
    std::ofstream f(xml_path.c_str());
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<UCIS xmlns=\"UCIS\" ucisVersion=\"1.0\" writtenBy=\"t\" writtenTime=\"2026-11-24T00:00:00\">\n"
      << "  <sourceFiles fileName=\"top.sv\" id=\"1\"/>\n"
      << "  <historyNodes historyNodeId=\"1\" logicalName=\"s\" testStatus=\"true\" "
         "date=\"2026-11-24T00:00:00\" toolCategory=\"UCIS:Sim\" ucisVersion=\"1.0\" "
         "vendorId=\"Accellera\" vendorTool=\"FC4SC\" vendorToolVersion=\"1.0\"/>\n"
      << "  <instanceCoverages name=\"top\" moduleName=\"dut\" key=\"1\" instanceId=\"1\">\n"
      << "    <id file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
      << "    <toggleCoverage weight=\"1\">\n"
      << "      <toggleObject name=\"clk\" key=\"2\" type=\"wire\" portDirection=\"in\">\n"
      << "        <id file=\"1\" line=\"5\" inlineCount=\"1\"/>\n"
      << "        <toggleBit name=\"clk\" key=\"3\">\n"
      << "          <toggle from=\"0\" to=\"1\"><bin><contents coverageCount=\"4\"/></bin></toggle>\n"
      << "          <toggle from=\"1\" to=\"0\"><bin><contents coverageCount=\"4\"/></bin></toggle>\n"
      << "        </toggleBit>\n"
      << "      </toggleObject>\n"
      << "    </toggleCoverage>\n"
      << "    <blockCoverage weight=\"1\">\n"
      << "      <statement><id file=\"1\" line=\"10\" inlineCount=\"1\"/><bin><contents coverageCount=\"1\"/></bin></statement>\n"
      << "    </blockCoverage>\n"
      << "    <assertionCoverage weight=\"1\">\n"
      << "      <assertion name=\"a_valid\" assertionKind=\"assert\">\n"
      << "        <passBin><contents coverageCount=\"5\"/></passBin>\n"
      << "        <failBin><contents coverageCount=\"1\"/></failBin>\n"
      << "        <vacuousBin><contents coverageCount=\"2\"/></vacuousBin>\n"
      << "        <disabledBin><contents coverageCount=\"0\"/></disabledBin>\n"
      << "        <attemptBin><contents coverageCount=\"8\"/></attemptBin>\n"
      << "        <activeBin><contents coverageCount=\"0\"/></activeBin>\n"
      << "        <peakActiveBin><contents coverageCount=\"1\"/></peakActiveBin>\n"
      << "      </assertion>\n"
      << "    </assertionCoverage>\n"
      << "    <covergroupCoverage weight=\"1\">\n"
      << "      <cgInstance name=\"cg\" key=\"4\">\n"
      << "        <options/>\n"
      << "        <cgId cgName=\"cg_t\" moduleName=\"dut\"><cginstSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/><cgSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/></cgId>\n"
      << "        <coverpoint name=\"cp_lv\" key=\"5\">\n"
      << "          <options/>\n"
      << "          <coverpointBin name=\"b_xz\" key=\"6\" type=\"default\">\n"
      << "            <range from=\"1\" to=\"1\"><contents coverageCount=\"3\"/></range>\n"
      << "            <userAttr key=\"fc4sc.range[0].from\" type=\"str\">00X1</userAttr>\n"
      << "            <userAttr key=\"fc4sc.range[0].to\" type=\"str\">00X1</userAttr>\n"
      << "          </coverpointBin>\n"
      << "        </coverpoint>\n"
      << "      </cgInstance>\n"
      << "    </covergroupCoverage>\n"
      << "  </instanceCoverages>\n"
      << "</UCIS>\n";
  }

  const std::string md = fc4sc::generate_markdown_report_from_xml_file(xml_path);
  const std::string html = fc4sc::generate_html_report_from_xml_file(xml_path);

  EXPECT_NE(md.find("Toggle Coverage"), std::string::npos);
  EXPECT_NE(md.find("clk"), std::string::npos);
  EXPECT_NE(md.find("Assertion Coverage"), std::string::npos);
  EXPECT_NE(md.find("a_valid"), std::string::npos);
  EXPECT_NE(md.find("Other UCIS Metrics"), std::string::npos);
  EXPECT_NE(md.find("Block"), std::string::npos);
  EXPECT_NE(md.find("00X1"), std::string::npos);

  EXPECT_NE(html.find("Toggle Coverage"), std::string::npos);
  EXPECT_NE(html.find("Assertion Coverage"), std::string::npos);
  EXPECT_NE(html.find("a_valid"), std::string::npos);
  EXPECT_NE(html.find("00X1"), std::string::npos);
}

namespace {

class parity_cg : public fc4sc::covergroup {
public:
  CG_CONS(parity_cg) {};
  int a = 0;
  int b = 0;
  COVERPOINT(int, cp_a, a) {
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  };
  COVERPOINT(int, cp_b, b) {
    bin<int>("c0", 0),
    bin<int>("c1", 1)
  };
  cross<int, int> cr_ab = cross<int, int>(this, "cr_ab", &cp_a, &cp_b);
};

class merge_inst_cg : public fc4sc::covergroup {
public:
  CG_CONS(merge_inst_cg) {
    type_option().merge_instances = true;
  };
  int x = 0;
  int y = 0;
  COVERPOINT(int, cp_x, x) {
    bin<int>("x0", 0),
    bin<int>("x1", 1)
  };
  COVERPOINT(int, cp_y, y) {
    bin<int>("y0", 0),
    bin<int>("y1", 1)
  };
  cross<int, int> cr_xy = cross<int, int>(this, "cr_xy", &cp_x, &cp_y);
};

class empty_test_scope : public fc4sc::scope {
public:
  SCOPE_DECL(empty_test_scope);
  empty_test_scope(const std::string& n, fc4sc::global* cntxt)
      : fc4sc::scope(n, __FILE__, __LINE__, __FILE__, __LINE__, cntxt) {}
};

} // namespace

TEST(report_generator, coverage_math_parity_with_global_and_empty_scopes) {
  // 1. merge_instances = true parity (live + XML)
  {
    auto ctx = fc4sc::global::create_new_context();
    merge_inst_cg m1("m1", __FILE__, __LINE__, ctx);
    merge_inst_cg m2("m2", __FILE__, __LINE__, ctx);
    m1.x = 0; m1.y = 0; m1.sample();
    m1.x = 0; m1.y = 1; m1.sample();
    m2.x = 1; m2.y = 0; m2.sample();
    m2.x = 1; m2.y = 1; m2.sample();

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 100.0, 1e-9);

    fc4sc::report_visitor rv;
    const auto rd_live = rv.build_from_global(ctx);
    EXPECT_NEAR(rd_live.overall_coverage_pct, gcov, 1e-9);

    const std::string xml_p = "report_merge_inst.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 2. option.goal clamping parity (non-strict + strict) for coverpoint, cross, covergroup
  {
    auto ctx = fc4sc::global::create_new_context();
    parity_cg cg("cg_goal", __FILE__, __LINE__, ctx);
    cg.option().goal = 50;
    cg.cp_a.option().goal = 50;
    cg.cp_b.option().goal = 50;
    cg.cr_ab.option().goal = 25;
    // 1 of 2 bins covered on cp_a & cp_b (50% >= goal 50 -> 100%), 1 of 4 on cr_ab (25% >= 25 -> 100%)
    cg.a = 0; cg.b = 0; cg.sample();

    const double inst_cov = cg.get_inst_coverage();
    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(inst_cov, 100.0, 1e-9);
    EXPECT_NEAR(gcov, 100.0, 1e-9);

    fc4sc::report_visitor rv;
    const auto rd_live = rv.build_from_global(ctx);
    ASSERT_EQ(rd_live.covergroups.size(), 1u);
    EXPECT_NEAR(rd_live.covergroups[0].coverage_pct, inst_cov, 1e-9);
    EXPECT_NEAR(rd_live.covergroups[0].coverpoints[0].coverage_pct, 100.0, 1e-9);
    EXPECT_NEAR(rd_live.covergroups[0].crosses[0].coverage_pct, 100.0, 1e-9);

    const std::string xml_p = "report_goal_clamp.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    ASSERT_EQ(rd_xml.covergroups.size(), 1u);
    EXPECT_NEAR(rd_xml.covergroups[0].coverage_pct, inst_cov, 1e-9);
    EXPECT_NEAR(rd_xml.covergroups[0].coverpoints[0].coverage_pct, 100.0, 1e-9);
    EXPECT_NEAR(rd_xml.covergroups[0].crosses[0].coverage_pct, 100.0, 1e-9);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 3. Disabled covergroup parity in XML (50% enabled + 100% disabled -> 75%)
  {
    auto ctx = fc4sc::global::create_new_context();
    parity_cg cg_en("cg_en", __FILE__, __LINE__, ctx);
    cg_en.cr_ab.option().weight = 0;
    cg_en.a = 0; cg_en.b = 0; cg_en.sample(); // 1/2 + 1/2 -> 50%

    fc4sc::dynamic_scope_factory dis_scp_fact("dis_scp_p");
    fc4sc::dynamic_covergroup_factory dis_cg_fact(dis_scp_fact, "dis_cg_p");
    auto dis_cvp = dis_cg_fact.create_coverpoint<int(int)>("x", [](int v) { return v; });
    dis_cvp.create_bin("b0", 0);
    dis_scp_fact.add_covergroup(dis_cg_fact, "dis_inst");
    fc4sc::dynamic_scope ds(dis_scp_fact, "ds_p", __FILE__, __LINE__, ctx);
    ds.get_covergroup("dis_inst").disable_cvg();

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 75.0, 1e-9);

    fc4sc::report_visitor rv;
    const auto rd_live = rv.build_from_global(ctx);
    EXPECT_NEAR(rd_live.overall_coverage_pct, gcov, 1e-9);
    EXPECT_EQ(rd_live.total_covergroups, 2u);
    EXPECT_EQ(rd_live.enabled_covergroups, 1u);

    const std::string xml_p = "report_disabled_cg.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    EXPECT_EQ(rd_xml.total_covergroups, 2u);
    EXPECT_EQ(rd_xml.enabled_covergroups, 1u);
    fc4sc::global::delete_context(ctx);
  }

  // 4. cg.option.weight == 0 with 0% coverpoints -> 100%, and option sync before sample()
  {
    auto ctx = fc4sc::global::create_new_context();
    parity_cg cg0("cg_w0", __FILE__, __LINE__, ctx);
    cg0.option().weight = 0;
    // 0 hits -> 0% coverpoints, weight == 0 -> get_inst_coverage() == 100%
    EXPECT_NEAR(cg0.get_inst_coverage(), 100.0, 1e-9);

    parity_cg cg_at("cg_at_least", __FILE__, __LINE__, ctx);
    cg_at.option().at_least = 2; // Mutated after construction, before sample/get_coverage!
    cg_at.a = 0; cg_at.b = 0; cg_at.sample(); // 1 hit < at_least(2) -> 0%

    auto find_cg = [](const fc4sc::report_data& rd, const std::string& nm) -> const fc4sc::report_covergroup* {
      for (size_t i = 0; i < rd.covergroups.size(); ++i) {
        if (rd.covergroups[i].name == nm) return &rd.covergroups[i];
      }
      return nullptr;
    };

    fc4sc::report_visitor rv;
    const auto rd_live = rv.build_from_global(ctx);
    ASSERT_EQ(rd_live.covergroups.size(), 2u);
    const auto* live_w0 = find_cg(rd_live, "cg_w0");
    const auto* live_at = find_cg(rd_live, "cg_at_least");
    ASSERT_NE(live_w0, nullptr);
    ASSERT_NE(live_at, nullptr);
    EXPECT_NEAR(live_w0->coverage_pct, 100.0, 1e-9);
    EXPECT_NEAR(live_at->coverpoints[0].coverage_pct, 0.0, 1e-9);

    const std::string xml_p = "report_w0_at_least.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    ASSERT_EQ(rd_xml.covergroups.size(), 2u);
    const auto* xml_w0 = find_cg(rd_xml, "cg_w0");
    const auto* xml_at = find_cg(rd_xml, "cg_at_least");
    ASSERT_NE(xml_w0, nullptr);
    ASSERT_NE(xml_at, nullptr);
    EXPECT_NEAR(xml_w0->coverage_pct, 100.0, 1e-9);
    EXPECT_NEAR(xml_at->coverpoints[0].coverage_pct, 0.0, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 5. cgInstance weights summing to 0 while <covergroupCoverage weight="1"> in XML -> 0% (averaged with 100% -> 50%)
  {
    const std::string xml_p = "report_zero_inst_weight.xml";
    {
      std::ofstream f(xml_p.c_str());
      f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<UCIS xmlns=\"UCIS\" ucisVersion=\"1.0\" writtenBy=\"t\" writtenTime=\"2026-11-24T00:00:00\">\n"
        << "  <sourceFiles fileName=\"top.sv\" id=\"1\"/>\n"
        << "  <historyNodes historyNodeId=\"1\" logicalName=\"s\" testStatus=\"true\" "
           "date=\"2026-11-24T00:00:00\" toolCategory=\"UCIS:Sim\" ucisVersion=\"1.0\" "
           "vendorId=\"Accellera\" vendorTool=\"FC4SC\" vendorToolVersion=\"1.0\"/>\n"
        << "  <instanceCoverages name=\"top\" moduleName=\"dut\" key=\"1\" instanceId=\"1\">\n"
        << "    <id file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
        << "    <covergroupCoverage weight=\"1\">\n"
        << "      <cgInstance name=\"cg0\" key=\"2\">\n"
        << "        <options weight=\"0\" goal=\"100\"/>\n"
        << "        <cgId cgName=\"cg_t0\" moduleName=\"dut\"><cginstSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/><cgSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/></cgId>\n"
        << "        <coverpoint name=\"cp0\" key=\"3\"><options weight=\"1\" goal=\"100\" at_least=\"1\"/><coverpointBin name=\"b0\" key=\"4\" type=\"default\"><range from=\"0\" to=\"0\"><contents coverageCount=\"1\"/></range></coverpointBin></coverpoint>\n"
        << "      </cgInstance>\n"
        << "    </covergroupCoverage>\n"
        << "    <covergroupCoverage weight=\"1\">\n"
        << "      <cgInstance name=\"cg1\" key=\"5\">\n"
        << "        <options weight=\"1\" goal=\"100\"/>\n"
        << "        <cgId cgName=\"cg_t1\" moduleName=\"dut\"><cginstSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/><cgSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/></cgId>\n"
        << "        <coverpoint name=\"cp1\" key=\"6\"><options weight=\"1\" goal=\"100\" at_least=\"1\"/><coverpointBin name=\"b1\" key=\"7\" type=\"default\"><range from=\"0\" to=\"0\"><contents coverageCount=\"1\"/></range></coverpointBin></coverpoint>\n"
        << "      </cgInstance>\n"
        << "    </covergroupCoverage>\n"
        << "  </instanceCoverages>\n"
        << "</UCIS>\n";
    }
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, 50.0, 1e-9);
  }

  // 7. include_empty_scopes wiring (live + XML)
  {
    auto ctx = fc4sc::global::create_new_context();
    empty_test_scope es("empty_scp_inst", ctx);
    (void)es;

    fc4sc::report_options opts_skip;
    opts_skip.include_empty_scopes = false;
    const std::string md_skip = fc4sc::generate_markdown_report(ctx, opts_skip);
    EXPECT_EQ(md_skip.find("Empty Scopes"), std::string::npos);
    EXPECT_EQ(md_skip.find("empty_scp_inst"), std::string::npos);

    fc4sc::report_options opts_inc;
    opts_inc.include_empty_scopes = true;
    const std::string md_inc = fc4sc::generate_markdown_report(ctx, opts_inc);
    const std::string html_inc = fc4sc::generate_html_report(ctx, opts_inc);
    EXPECT_NE(md_inc.find("Empty Scopes"), std::string::npos);
    EXPECT_NE(md_inc.find("empty_scp_inst"), std::string::npos);
    EXPECT_NE(html_inc.find("Empty Scopes"), std::string::npos);
    EXPECT_NE(html_inc.find("empty_scp_inst"), std::string::npos);

    const std::string xml_p = "report_empty_scope.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const std::string md_xml_skip = fc4sc::generate_markdown_report_from_xml_file(xml_p, opts_skip);
    const std::string md_xml_inc = fc4sc::generate_markdown_report_from_xml_file(xml_p, opts_inc);
    EXPECT_EQ(md_xml_skip.find("Empty Scopes"), std::string::npos);
    EXPECT_NE(md_xml_inc.find("Empty Scopes"), std::string::npos);
    EXPECT_NE(md_xml_inc.find("empty_scp_inst"), std::string::npos);
    fc4sc::global::delete_context(ctx);
  }
}

namespace {

class merge_goal_cg : public fc4sc::covergroup {
public:
  CG_CONS(merge_goal_cg) {
    type_option().merge_instances = true;
    type_option().goal = 50;
  };
  int x = 0;
  COVERPOINT(int, cp_x, x) {
    bin<int>("x0", 0),
    bin<int>("x1", 1)
  };
};

} // namespace

TEST(report_generator, xml_disabled_cg_disambiguation_grouping_type_goal_and_empty_cg_context) {
  // 1. Real custom <userAttr key="smoke_tag" type="str">smoke_tag</userAttr> must NOT be misclassified
  {
    const std::string xml_p = "report_custom_userattr.xml";
    {
      std::ofstream f(xml_p.c_str());
      f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<UCIS xmlns=\"UCIS\" ucisVersion=\"1.0\" writtenBy=\"t\" writtenTime=\"2026-11-24T00:00:00\">\n"
        << "  <sourceFiles fileName=\"top.sv\" id=\"1\"/>\n"
        << "  <historyNodes historyNodeId=\"1\" logicalName=\"s\" testStatus=\"true\" "
           "date=\"2026-11-24T00:00:00\" toolCategory=\"UCIS:Sim\" ucisVersion=\"1.0\" "
           "vendorId=\"Accellera\" vendorTool=\"FC4SC\" vendorToolVersion=\"1.0\"/>\n"
        << "  <instanceCoverages name=\"top\" moduleName=\"dut\" key=\"1\" instanceId=\"1\">\n"
        << "    <id file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
        << "    <covergroupCoverage weight=\"1\">\n"
        << "      <cgInstance name=\"cg_en\" key=\"2\">\n"
        << "        <options weight=\"1\" goal=\"100\"/>\n"
        << "        <cgId cgName=\"cg_en_t\" moduleName=\"dut\"><cginstSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/><cgSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/></cgId>\n"
        << "        <coverpoint name=\"cp\" key=\"3\"><options weight=\"1\" goal=\"100\" at_least=\"1\"/><coverpointBin name=\"b0\" key=\"4\" type=\"default\"><range from=\"0\" to=\"0\"><contents coverageCount=\"0\"/></range></coverpointBin></coverpoint>\n"
        << "      </cgInstance>\n"
        << "    </covergroupCoverage>\n"
        << "    <userAttr key=\"fc4sc.disabled_cg\" type=\"str\">dis_cg_t</userAttr>\n"
        << "    <userAttr key=\"smoke_tag\" type=\"str\">smoke_tag</userAttr>\n"
        << "  </instanceCoverages>\n"
        << "</UCIS>\n";
    }
    const auto rd = fc4sc::build_from_xml_file(xml_p);
    EXPECT_EQ(rd.total_covergroups, 2u);
    EXPECT_EQ(rd.enabled_covergroups, 1u);
    EXPECT_NEAR(rd.overall_coverage_pct, 50.0, 1e-9);
  }

  // 2. merge_instances=true with 1 enabled (50%) + 1 disabled instance in different scope -> 50%
  {
    auto ctx = fc4sc::global::create_new_context();
    fc4sc::dynamic_scope_factory sf("m_dis_scp");
    fc4sc::dynamic_covergroup_factory cf(sf, "m_dis_cg");
    auto cp = cf.create_coverpoint<int(int)>("x", [](int v) { return v; });
    cp.create_bin("b0", 0);
    cp.create_bin("b1", 1);
    sf.add_covergroup(cf, "cg");

    fc4sc::dynamic_scope ds1(sf, "ds1", __FILE__, __LINE__, ctx);
    fc4sc::dynamic_scope ds2(sf, "ds2", __FILE__, __LINE__, ctx);
    ds1.get_covergroup("cg").type_option().merge_instances = true;
    int vx = 0;
    cp.bind_sample(ds1.get_covergroup("cg"), vx);
    ds1.get_covergroup("cg").sample(); // 50%
    ds2.get_covergroup("cg").disable_cvg();

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 50.0, 1e-9);
    const std::string xml_p = "report_merge_inst_cross_scope_dis.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 3. Two disabled instances of same type + one 0% enabled type -> 50% (grouped by type, not per-inst)
  {
    auto ctx = fc4sc::global::create_new_context();
    parity_cg cg_en("cg_en", __FILE__, __LINE__, ctx); // 0%

    fc4sc::dynamic_scope_factory sf("d2_scp");
    fc4sc::dynamic_covergroup_factory cf(sf, "d2_cg");
    auto cp = cf.create_coverpoint<int(int)>("x", [](int v) { return v; });
    cp.create_bin("b0", 0);
    sf.add_covergroup(cf, "cg");
    fc4sc::dynamic_scope ds1(sf, "ds1", __FILE__, __LINE__, ctx);
    fc4sc::dynamic_scope ds2(sf, "ds2", __FILE__, __LINE__, ctx);
    ds1.get_covergroup("cg").disable_cvg();
    ds2.get_covergroup("cg").disable_cvg();

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 50.0, 1e-9);
    const std::string xml_p = "report_two_dis_same_type.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 4. Cross-scope same-type averaging with disabled instance and type_option.weight=2 -> 75%
  {
    auto ctx = fc4sc::global::create_new_context();
    fc4sc::dynamic_scope_factory sf("cs_scp");
    fc4sc::dynamic_covergroup_factory cf(sf, "cs_cg");
    auto cp = cf.create_coverpoint<int(int)>("x", [](int v) { return v; });
    cp.create_bin("b0", 0);
    cp.create_bin("b1", 1);
    sf.add_covergroup(cf, "cg");

    fc4sc::dynamic_scope s1(sf, "s1", __FILE__, __LINE__, ctx);
    fc4sc::dynamic_scope s2(sf, "s2", __FILE__, __LINE__, ctx);
    s1.get_covergroup("cg").type_option().weight = 2;
    int vx = 0;
    cp.bind_sample(s1.get_covergroup("cg"), vx);
    s1.get_covergroup("cg").sample(); // 50%
    s2.get_covergroup("cg").disable_cvg(); // 100%

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 75.0, 1e-9);
    const std::string xml_p = "report_cross_scope_dis_avg.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 5. merge_instances=true with type_option.goal=50 clamping (non-strict and strict_sv_options)
  for (int strict = 0; strict < 2; ++strict) {
    auto ctx = fc4sc::global::create_new_context();
    merge_goal_cg c1("c1", __FILE__, __LINE__, ctx);
    merge_goal_cg c2("c2", __FILE__, __LINE__, ctx);
    if (strict) {
      c1.type_option().strict_sv_options = true;
      c1.option().per_instance = false;
      c2.option().per_instance = false;
    }
    c1.x = 0; c1.sample(); // 50% raw >= type_option.goal(50) -> 100%
    c2.x = 0; c2.sample();

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 100.0, 1e-9);
    const std::string xml_p = strict ? "report_mgoal_strict.xml" : "report_mgoal_nonstrict.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }

  // 6. Empty-of-covergroups context with registered scope -> 0% (not 100%)
  {
    auto ctx = fc4sc::global::create_new_context();
    empty_test_scope es("empty_scp_cg0", ctx);
    (void)es;

    const double gcov = fc4sc::global::get_coverage(ctx);
    EXPECT_NEAR(gcov, 0.0, 1e-9);

    fc4sc::report_visitor rv;
    const auto rd_live = rv.build_from_global(ctx);
    EXPECT_NEAR(rd_live.overall_coverage_pct, gcov, 1e-9);

    const std::string xml_p = "report_empty_cg_scope.xml";
    xml_printer::coverage_save(xml_p, ctx);
    const auto rd_xml = fc4sc::build_from_xml_file(xml_p);
    EXPECT_NEAR(rd_xml.overall_coverage_pct, gcov, 1e-9);
    fc4sc::global::delete_context(ctx);
  }
}
