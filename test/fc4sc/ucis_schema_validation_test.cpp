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
#include <sstream>
#include <string>

#include <fc4sc.hpp>
#include "gtest/gtest.h"

namespace {

std::string read_file_text(const std::string& path) {
  std::ifstream in(path.c_str());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

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

class cross_before_cvp_cvg : public covergroup {
public:
  CG_CONS(cross_before_cvp_cvg) {
    option().per_instance = true;
  };

  int a = 0;
  int b = 0;
  int c = 0;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("zero", 0),
    bin<int>("one", 1)
  };

  cross<int, int> crs_ab = cross<int, int>(this, "crs_ab", &cvp_a, &cvp_b);

  // Declared after crs_ab: UCIS CGINSTANCE requires all <coverpoint> elements
  // to precede all <cross> elements.
  COVERPOINT(int, cvp_c, c) {
    bin<int>("low", interval(0, 3)),
    bin<int>("high", interval(4, 7))
  };
};

class empty_and_unhit_default_cvg : public covergroup {
public:
  CG_CONS(empty_and_unhit_default_cvg) {
    option().per_instance = true;
  };

  int x = 0;
  int y = 0;

  // Filtered to zero bins by with(); UCIS COVERPOINT still requires >= 1 coverpointBin.
  COVERPOINT(int, empty_with, x) {
    bin<int>("impossible", interval(0, 3)).with([](int v) { return v > 10; })
  };

  // Scalar default_bin with 0 hits has no hit ranges yet; UCIS COVERPOINT still requires >= 1 coverpointBin.
  COVERPOINT(int, unhit_default_only, y) {
    default_bin<int>("others")
  };
};

class empty_cvg : public covergroup {
public:
  CG_CONS(empty_cvg) {
    option().per_instance = true;
  };
};

} // namespace

TEST(ucis_schema_validation, coverpoints_precede_crosses_in_cginstance) {
  const std::string xml_path = "ucis_schema_cross_before_cvp.xml";
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_before_cvp_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.a = 1;
    cvg.b = 0;
    cvg.c = 5;
    cvg.sample();
    xml_printer::coverage_save(xml_path, cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  const std::string xml = read_file_text(xml_path);
  const size_t cvp_c_pos = xml.find("<coverpoint name=\"cvp_c\"");
  const size_t crs_pos = xml.find("<cross name=\"crs_ab\"");
  ASSERT_NE(cvp_c_pos, std::string::npos);
  ASSERT_NE(crs_pos, std::string::npos);
  EXPECT_LT(cvp_c_pos, crs_pos)
      << "Expected <coverpoint name=\"cvp_c\"> before <cross name=\"crs_ab\">";

  expect_valid_ucis_schema(xml_path);
}

TEST(ucis_schema_validation, empty_coverpoints_and_covergroups_validate_and_roundtrip) {
  const std::string run1_xml = "ucis_schema_empty_run1.xml";
  const std::string run2_xml = "ucis_schema_empty_run2.xml";
  const std::string merged_xml = "ucis_schema_empty_merged.xml";

  // Run 1: 0 samples, plus an empty dynamic covergroup with no coverpoints.
  auto cntxt1 = fc4sc::global::create_new_context();
  {
    empty_and_unhit_default_cvg cvg("cvg", __FILE__, __LINE__, cntxt1);
    empty_cvg empty_inst("empty_dyn", __FILE__, __LINE__, cntxt1);
    (void)empty_inst;
    xml_printer::coverage_save(run1_xml, cntxt1);
  }
  fc4sc::global::delete_context(cntxt1);

  const std::string xml1 = read_file_text(run1_xml);
  const size_t empty_cvp = xml1.find("<coverpoint name=\"empty_with\"");
  ASSERT_NE(empty_cvp, std::string::npos);
  const std::string empty_sec =
      xml1.substr(empty_cvp, xml1.find("</coverpoint>", empty_cvp) - empty_cvp);
  EXPECT_NE(empty_sec.find("<coverpointBin"), std::string::npos);
  EXPECT_EQ(xml1.find("name=\"empty_dyn\""), std::string::npos);
  expect_valid_ucis_schema(run1_xml);

  // Loading run1 back into a fresh model must not report missing placeholder bins.
  auto cntxt_load = fc4sc::global::create_new_context();
  {
    empty_and_unhit_default_cvg cvg("cvg", __FILE__, __LINE__, cntxt_load);
    std::ostringstream err_capture;
    std::streambuf* old_err = std::cerr.rdbuf(err_capture.rdbuf());
    fc4sc::load_coverage_db(run1_xml, cntxt_load);
    std::cerr.rdbuf(old_err);
    EXPECT_EQ(err_capture.str(), "");
    EXPECT_EQ(cvg.unhit_default_only.get_default_bins_base()[0]->get_hitcount(), 0u);
  }
  fc4sc::global::delete_context(cntxt_load);

  // Run 2: sample y = 7 so unhit_default_only now has a real ignore bin "others",
  // then merge run1 + run2 and verify schema + clean merged bin set.
  auto cntxt2 = fc4sc::global::create_new_context();
  {
    empty_and_unhit_default_cvg cvg("cvg", __FILE__, __LINE__, cntxt2);
    cvg.y = 7;
    cvg.sample();
    xml_printer::coverage_save(run2_xml, cntxt2);
  }
  fc4sc::global::delete_context(cntxt2);
  expect_valid_ucis_schema(run2_xml);

  fc4sc::merge_coverage_db({run1_xml, run2_xml}, merged_xml);
  expect_valid_ucis_schema(merged_xml);

  const std::string merged = read_file_text(merged_xml);
  const size_t def_cvp = merged.find("<coverpoint name=\"unhit_default_only\"");
  ASSERT_NE(def_cvp, std::string::npos);
  const std::string def_sec =
      merged.substr(def_cvp, merged.find("</coverpoint>", def_cvp) - def_cvp);
  EXPECT_NE(def_sec.find("name=\"others\""), std::string::npos);
  EXPECT_EQ(def_sec.find("name=\"\""), std::string::npos);
}
