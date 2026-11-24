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

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

std::string ucis_xsd_path() {
  std::string file = __FILE__;
  const size_t pos = file.rfind("test/fc4sc/");
  if (pos != std::string::npos) {
    return file.substr(0, pos) + "ucis/UCIS.xsd";
  }
  return "../../ucis/UCIS.xsd";
}

bool has_xmllint() {
  return std::system("xmllint --version >/dev/null 2>&1") == 0;
}

void expect_valid_ucis_schema(const std::string& xml_path) {
  const std::string xsd = ucis_xsd_path();
  std::ifstream check(xsd.c_str());
  ASSERT_TRUE(check.good()) << "Missing schema at " << xsd;
  if (has_xmllint()) {
    const std::string cmd =
        "xmllint --schema \"" + xsd + "\" --noout \"" + xml_path + "\" >/dev/null 2>&1";
    EXPECT_EQ(std::system(cmd.c_str()), 0)
        << "UCIS.xsd schema validation failed for " << xml_path;
  }
}

std::unique_ptr<fc4sc::ucis_db::xml_node> make_elem(const std::string& name) {
  std::unique_ptr<fc4sc::ucis_db::xml_node> n(new fc4sc::ucis_db::xml_node());
  n->name = name;
  return n;
}

std::string build_7metric_doc(int file_order,
                             uint64_t toggle_01,
                             uint64_t stmt_hits,
                             uint64_t block_hits,
                             uint64_t cond_hits,
                             uint64_t branch_hits,
                             uint64_t fsm_state_hits,
                             uint64_t fsm_trans_hits,
                             uint64_t asrt_pass,
                             uint64_t asrt_fail,
                             uint64_t asrt_peak_active,
                             uint64_t cg_hits,
                             const std::string& extra_param) {
  std::ostringstream ss;
  ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
     << "<UCIS xmlns=\"UCIS\" ucisVersion=\"1.0\" writtenBy=\"test\" writtenTime=\"2026-11-24T00:00:00\">\n";
  if (file_order == 0) {
    ss << "  <sourceFiles fileName=\"alpha.sv\" id=\"1\"/>\n"
       << "  <sourceFiles fileName=\"beta.sv\" id=\"2\"/>\n";
  } else {
    ss << "  <sourceFiles fileName=\"beta.sv\" id=\"1\"/>\n"
       << "  <sourceFiles fileName=\"alpha.sv\" id=\"2\"/>\n";
  }
  const char* beta_id = (file_order == 0) ? "2" : "1";
  const char* alpha_id = (file_order == 0) ? "1" : "2";

  ss << "  <historyNodes historyNodeId=\"10\" logicalName=\"sim\" testStatus=\"true\" "
        "date=\"2026-11-24T00:00:00\" toolCategory=\"UCIS:Sim\" ucisVersion=\"1.0\" "
        "vendorId=\"Accellera\" vendorTool=\"FC4SC\" vendorToolVersion=\"1.0\"/>\n"
     << "  <instanceCoverages name=\"top\" moduleName=\"dut\" key=\"1\" instanceId=\"1\">\n"
     << "    <designParameter><name>WIDTH</name><value>32</value></designParameter>\n";
  if (!extra_param.empty()) {
    ss << "    <designParameter><name>" << extra_param << "</name><value>1</value></designParameter>\n";
  }
  ss << "    <id file=\"" << alpha_id << "\" line=\"10\" inlineCount=\"1\"/>\n"
     << "    <toggleCoverage metricMode=\"2state\" weight=\"1\">\n"
     << "      <toggleObject name=\"sig_a\" key=\"2\" type=\"wire\" portDirection=\"in\">\n"
     << "        <id file=\"" << beta_id << "\" line=\"20\" inlineCount=\"1\"/>\n"
     << "        <toggleBit name=\"sig_a\" key=\"3\">\n"
     << "          <index>0</index>\n"
     << "          <toggle from=\"0\" to=\"1\"><bin><contents coverageCount=\"" << toggle_01 << "\"/></bin></toggle>\n"
     << "          <toggle from=\"1\" to=\"0\"><bin><contents coverageCount=\"1\"/></bin></toggle>\n"
     << "        </toggleBit>\n"
     << "      </toggleObject>\n"
     << "      <metricMode metricMode=\"2state\"/>\n"
     << "    </toggleCoverage>\n"
     << "    <blockCoverage weight=\"1\">\n"
     << "      <block parentProcess=\"always_ff\">\n"
     << "        <statementId file=\"" << beta_id << "\" line=\"31\" inlineCount=\"1\"/>\n"
     << "        <hierarchicalBlock parentProcess=\"if_true\">\n"
     << "          <blockBin><contents coverageCount=\"" << stmt_hits << "\"/></blockBin>\n"
     << "          <blockId file=\"" << beta_id << "\" line=\"32\" inlineCount=\"1\"/>\n"
     << "        </hierarchicalBlock>\n"
     << "        <blockBin><contents coverageCount=\"" << block_hits << "\"/></blockBin>\n"
     << "        <blockId file=\"" << beta_id << "\" line=\"30\" inlineCount=\"1\"/>\n"
     << "      </block>\n"
     << "    </blockCoverage>\n"
     << "    <conditionCoverage weight=\"1\">\n"
     << "      <expr name=\"c0\" key=\"4\" exprString=\"a &amp;&amp; b\" index=\"0\" width=\"2\">\n"
     << "        <id file=\"" << beta_id << "\" line=\"40\" inlineCount=\"1\"/>\n"
     << "        <subExpr>a</subExpr>\n"
     << "        <bin alias=\"11\"><contents coverageCount=\"" << cond_hits << "\"/><userAttr key=\"cond_note\" type=\"str\">" << (file_order == 0 ? "c_run1" : "c_run2") << "</userAttr></bin>\n"
     << "      </expr>\n"
     << "    </conditionCoverage>\n"
     << "    <branchCoverage weight=\"1\">\n"
     << "      <statement branchExpr=\"en\" statementType=\"if\">\n"
     << "        <id file=\"" << beta_id << "\" line=\"50\" inlineCount=\"1\"/>\n"
     << "        <branch>\n"
     << "          <id file=\"" << beta_id << "\" line=\"51\" inlineCount=\"1\"/>\n"
     << "          <branchBin><contents coverageCount=\"" << branch_hits << "\"/></branchBin>\n"
     << "        </branch>\n"
     << "      </statement>\n"
     << "    </branchCoverage>\n"
     << "    <fsmCoverage weight=\"1\">\n"
     << "      <fsm name=\"state_q\" type=\"enum\" width=\"2\">\n"
     << "        <state stateName=\"IDLE\" stateValue=\"0\"><stateBin><contents coverageCount=\"" << fsm_state_hits << "\"/><userAttr key=\"st_note\" type=\"str\">" << (file_order == 0 ? "s_run1" : "s_run2") << "</userAttr></stateBin></state>\n"
     << "        <stateTransition><state>IDLE</state><state>RUN</state><transitionBin><contents coverageCount=\"" << fsm_trans_hits << "\"/><userAttr key=\"tr_note\" type=\"str\">" << (file_order == 0 ? "t_run1" : "t_run2") << "</userAttr></transitionBin></stateTransition>\n"
     << "      </fsm>\n"
     << "    </fsmCoverage>\n"
     << "    <assertionCoverage weight=\"1\">\n"
     << "      <assertion name=\"p_req_ack\" assertionKind=\"assert\">\n"
     << "        <passBin><contents coverageCount=\"" << asrt_pass << "\"/></passBin>\n"
     << "        <failBin><contents coverageCount=\"" << asrt_fail << "\"/></failBin>\n"
     << "        <vacuousBin><contents coverageCount=\"2\"/></vacuousBin>\n"
     << "        <disabledBin><contents coverageCount=\"1\"/></disabledBin>\n"
     << "        <attemptBin><contents coverageCount=\"" << (asrt_pass + asrt_fail + 3) << "\"/></attemptBin>\n"
     << "        <activeBin><contents coverageCount=\"0\"/></activeBin>\n"
     << "        <peakActiveBin><contents coverageCount=\"" << asrt_peak_active << "\"/></peakActiveBin>\n"
     << "      </assertion>\n"
     << "    </assertionCoverage>\n"
     << "    <covergroupCoverage weight=\"1\">\n"
     << "      <cgInstance name=\"cg\" key=\"5\">\n"
     << "        <options/>\n"
     << "        <cgId cgName=\"my_cg\" moduleName=\"dut\">\n"
     << "          <cginstSourceId file=\"" << alpha_id << "\" line=\"60\" inlineCount=\"1\"/>\n"
     << "          <cgSourceId file=\"" << alpha_id << "\" line=\"60\" inlineCount=\"1\"/>\n"
     << "        </cgId>\n"
     << "        <coverpoint name=\"cp\" key=\"6\">\n"
     << "          <options/>\n"
     << "          <coverpointBin name=\"b0\" key=\"7\" type=\"default\">\n"
     << "            <range from=\"0\" to=\"0\"><contents coverageCount=\"" << cg_hits << "\"/></range>\n"
     << "            <userAttr key=\"cg_bin_note\" type=\"str\">" << (file_order == 0 ? "gb_run1" : "gb_run2") << "</userAttr>\n"
     << "          </coverpointBin>\n"
     << "          <userAttr key=\"cg_cp_note\" type=\"str\">" << (file_order == 0 ? "cp_run1" : "cp_run2") << "</userAttr>\n"
     << "        </coverpoint>\n"
     << "        <cross name=\"cr\" key=\"8\">\n"
     << "          <options/>\n"
     << "          <crossExpr>cp</crossExpr>\n"
     << "          <crossBin name=\"&lt;0&gt;\" key=\"9\">\n"
     << "            <index>0</index>\n"
     << "            <contents coverageCount=\"" << cg_hits << "\"/>\n"
     << "            <userAttr key=\"cg_crb_note\" type=\"str\">" << (file_order == 0 ? "crb_run1" : "crb_run2") << "</userAttr>\n"
     << "          </crossBin>\n"
     << "          <userAttr key=\"cg_cr_note\" type=\"str\">" << (file_order == 0 ? "cr_run1" : "cr_run2") << "</userAttr>\n"
     << "        </cross>\n"
     << "        <userAttr key=\"cg_inst_note\" type=\"str\">" << (file_order == 0 ? "cgi_run1" : "cgi_run2") << "</userAttr>\n"
     << "      </cgInstance>\n"
     << "    </covergroupCoverage>\n"
     << "    <userAttr key=\"note\" type=\"str\">run</userAttr>\n"
     << "  </instanceCoverages>\n"
     << "</UCIS>\n";
  return ss.str();
}

} // namespace

TEST(ucis_7metric_merge, insert_child_enforces_xsd_sequence_all_metrics) {
  fc4sc::ucis_db::xml_node inst;
  inst.name = "instanceCoverages";

  // Insert in reverse XSD order; insert_child must sort into exact UCIS.xsd order.
  fc4sc::ucis_db::insert_child(inst, make_elem("userAttr"));
  fc4sc::ucis_db::insert_child(inst, make_elem("covergroupCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("assertionCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("fsmCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("branchCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("conditionCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("blockCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("toggleCoverage"));
  fc4sc::ucis_db::insert_child(inst, make_elem("id"));
  fc4sc::ucis_db::insert_child(inst, make_elem("designParameter"));

  std::vector<std::string> got;
  for (const auto& c : inst.children) got.push_back(c->name);
  const std::vector<std::string> expected = {
    "designParameter", "id", "toggleCoverage", "blockCoverage",
    "conditionCoverage", "branchCoverage", "fsmCoverage",
    "assertionCoverage", "covergroupCoverage", "userAttr"
  };
  EXPECT_EQ(got, expected);

  // Verify nested parent-specific sequences where element names collide across metrics.
  fc4sc::ucis_db::xml_node blk;
  blk.name = "block";
  fc4sc::ucis_db::insert_child(blk, make_elem("userAttr"));
  fc4sc::ucis_db::insert_child(blk, make_elem("blockId"));
  fc4sc::ucis_db::insert_child(blk, make_elem("blockBin"));
  fc4sc::ucis_db::insert_child(blk, make_elem("hierarchicalBlock"));
  fc4sc::ucis_db::insert_child(blk, make_elem("statementId"));
  std::vector<std::string> blk_got;
  for (const auto& c : blk.children) blk_got.push_back(c->name);
  EXPECT_EQ(blk_got, (std::vector<std::string>{"statementId", "hierarchicalBlock", "blockBin", "blockId", "userAttr"}));

  fc4sc::ucis_db::xml_node asrt;
  asrt.name = "assertion";
  fc4sc::ucis_db::insert_child(asrt, make_elem("userAttr"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("peakActiveBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("activeBin"));
  asrt.children.clear();
  fc4sc::ucis_db::insert_child(asrt, make_elem("userAttr"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("peakActiveBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("activeBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("attemptBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("disabledBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("vacuousBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("failBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("passBin"));
  fc4sc::ucis_db::insert_child(asrt, make_elem("coverBin"));
  std::vector<std::string> asrt_got;
  for (const auto& c : asrt.children) asrt_got.push_back(c->name);
  EXPECT_EQ(asrt_got, (std::vector<std::string>{
    "coverBin", "passBin", "failBin", "vacuousBin",
    "disabledBin", "attemptBin", "activeBin", "peakActiveBin", "userAttr"
  }));
}

TEST(ucis_7metric_merge, merge_preserves_all_7_metrics_and_validates_xsd) {
  const std::string p1 = "ucis_7metric_1.xml";
  const std::string p2 = "ucis_7metric_2.xml";
  const std::string pout = "ucis_7metric_merged.xml";

  {
    std::ofstream f1(p1.c_str());
    f1 << build_7metric_doc(0, 3, 5, 7, 2, 4, 6, 8, 10, 1, 4, 11, "");
    std::ofstream f2(p2.c_str());
    f2 << build_7metric_doc(1, 4, 6, 8, 3, 5, 7, 9, 20, 2, 7, 13, "MODE");
  }
  expect_valid_ucis_schema(p1);
  expect_valid_ucis_schema(p2);

  fc4sc::merge_coverage_db({p1, p2}, pout);
  expect_valid_ucis_schema(pout);

  auto merged = fc4sc::ucis_db::read_xml_file(pout);
  auto insts = merged->elements("instanceCoverages");
  ASSERT_EQ(insts.size(), 1u);
  auto* inst = insts[0];

  // Verify designParameter merged
  ASSERT_EQ(inst->elements("designParameter").size(), 2u);

  // Verify toggleCoverage merged (0->1: 3 + 4 = 7, 1->0: 1 + 1 = 2)
  auto togs = inst->elements("toggleCoverage");
  ASSERT_EQ(togs.size(), 1u);
  auto* tbit = togs[0]->element("toggleObject")->element("toggleBit");
  auto t_elems = tbit->elements("toggle");
  ASSERT_EQ(t_elems.size(), 2u);
  EXPECT_EQ(t_elems[0]->element("bin")->element("contents")->get("coverageCount"), "7");
  EXPECT_EQ(t_elems[1]->element("bin")->element("contents")->get("coverageCount"), "2");

  // Verify blockCoverage merged with cross-file statementId/blockId remapping
  auto blks = inst->elements("blockCoverage");
  ASSERT_EQ(blks.size(), 1u);
  auto* blk = blks[0]->element("block");
  EXPECT_EQ(blk->element("blockId")->get("file"), "2");
  EXPECT_EQ(blk->element("statementId")->get("file"), "2");
  EXPECT_EQ(blk->element("blockBin")->element("contents")->get("coverageCount"), "15");
  EXPECT_EQ(blk->element("hierarchicalBlock")->element("blockBin")->element("contents")->get("coverageCount"), "11");
  EXPECT_EQ(blk->element("hierarchicalBlock")->element("blockId")->get("file"), "2");

  // Verify conditionCoverage, branchCoverage, fsmCoverage + leaf bin userAttr preservation
  auto* cond_bin = inst->element("conditionCoverage")->element("expr")->element("bin");
  EXPECT_EQ(cond_bin->element("contents")->get("coverageCount"), "5");
  EXPECT_EQ(cond_bin->elements("userAttr").size(), 2u);
  EXPECT_EQ(inst->element("branchCoverage")->element("statement")->element("branch")->element("branchBin")->element("contents")->get("coverageCount"), "9");
  auto* st_bin = inst->element("fsmCoverage")->element("fsm")->element("state")->element("stateBin");
  EXPECT_EQ(st_bin->element("contents")->get("coverageCount"), "13");
  EXPECT_EQ(st_bin->elements("userAttr").size(), 2u);
  auto* tr_bin = inst->element("fsmCoverage")->element("fsm")->element("stateTransition")->element("transitionBin");
  EXPECT_EQ(tr_bin->element("contents")->get("coverageCount"), "17");
  EXPECT_EQ(tr_bin->elements("userAttr").size(), 2u);

  // Verify assertionCoverage: summed counters + std::max for peakActiveBin (max(4,7)=7)
  auto* asrt = inst->element("assertionCoverage")->element("assertion");
  EXPECT_EQ(asrt->element("passBin")->element("contents")->get("coverageCount"), "30");
  EXPECT_EQ(asrt->element("failBin")->element("contents")->get("coverageCount"), "3");
  EXPECT_EQ(asrt->element("vacuousBin")->element("contents")->get("coverageCount"), "4");
  EXPECT_EQ(asrt->element("disabledBin")->element("contents")->get("coverageCount"), "2");
  EXPECT_EQ(asrt->element("activeBin")->element("contents")->get("coverageCount"), "0");
  EXPECT_EQ(asrt->element("peakActiveBin")->element("contents")->get("coverageCount"), "7");

  // Verify covergroupCoverage + coverpointBin/coverpoint/crossBin/cross/cgInstance leaf userAttr preservation
  auto* cg_inst = inst->element("covergroupCoverage")->element("cgInstance");
  auto* cp_node = cg_inst->element("coverpoint");
  auto* cp_bin = cp_node->element("coverpointBin");
  auto* cr_node = cg_inst->element("cross");
  auto* cr_bin = cr_node->element("crossBin");
  EXPECT_EQ(cp_bin->element("range")->element("contents")->get("coverageCount"), "24");
  EXPECT_EQ(cp_bin->elements("userAttr").size(), 2u);
  EXPECT_EQ(cp_node->elements("userAttr").size(), 2u);
  EXPECT_EQ(cr_bin->element("contents")->get("coverageCount"), "24");
  EXPECT_EQ(cr_bin->elements("userAttr").size(), 2u);
  EXPECT_EQ(cr_node->elements("userAttr").size(), 2u);
  EXPECT_EQ(cg_inst->elements("userAttr").size(), 2u);
}
