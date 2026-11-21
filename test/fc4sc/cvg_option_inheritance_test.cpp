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
#include "gtest/gtest.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using fc4sc::ucis_db::xml_node;

std::string read_xml_text(const std::string& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const xml_node* find_node_by_name(const xml_node& node,
                                  const std::string& tag,
                                  const std::string& name) {
  if (node.name == tag && node.get("name") == name)
    return &node;
  for (auto& child : node.children) {
    const xml_node* found = find_node_by_name(*child, tag, name);
    if (found != nullptr)
      return found;
  }
  return nullptr;
}

class option_inh_cg : public covergroup {
public:
  CG_CONS(option_inh_cg) {};

  int a = 0;
  int b = 0;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("low", interval(0, 5)),
    bin<int>("high", interval(3, 8))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b1", 1),
    bin<int>("b2", 2)
  };

  cross<int, int> crs = cross<int, int>(this, "crs", &cvp_a, &cvp_b);

  void sample_vals(int va, int vb) {
    a = va;
    b = vb;
    covergroup::sample();
  }
};

class ctor_option_cg : public covergroup {
public:
  CG_CONS(ctor_option_cg) {
    option().at_least = 3;
    option().auto_bin_max = 24;
    option().detect_overlap = true;
    option().cross_num_print_missing = 2;
    option().weight = 7;
    option().goal = 85;
    option().comment = "cg_only";
  };

  int a = 0;
  int b = 0;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  };

  cross<int, int> crs = cross<int, int>(this, "crs", &cvp_a, &cvp_b);
};

} // namespace

TEST(cvg_option_inheritance, constructor_and_after_construction_sync) {
  auto cntxt = fc4sc::global::create_new_context();
  ctor_option_cg cg("cg", __FILE__, __LINE__, cntxt);

  EXPECT_EQ(cg.cvp_a.option().at_least, 3u);
  EXPECT_EQ(cg.cvp_a.option().auto_bin_max, 24u);
  EXPECT_TRUE(cg.cvp_a.option().detect_overlap);
  EXPECT_EQ(cg.cvp_b.option().at_least, 3u);
  EXPECT_EQ(cg.cvp_b.option().auto_bin_max, 24u);
  EXPECT_TRUE(cg.cvp_b.option().detect_overlap);
  EXPECT_EQ(cg.crs.option().at_least, 3u);
  EXPECT_EQ(cg.crs.option().cross_num_print_missing, 2u);

  // Non-inherited covergroup fields remain at defaults on children
  EXPECT_EQ(cg.cvp_a.option().weight, 1u);
  EXPECT_EQ(cg.cvp_a.option().goal, 100u);
  EXPECT_EQ(cg.cvp_a.option().comment, "");
  EXPECT_EQ(cg.crs.option().weight, 1u);
  EXPECT_EQ(cg.crs.option().goal, 100u);
  EXPECT_EQ(cg.crs.option().comment, "");

  // Re-assignment after construction propagates to non-overridden children
  cg.option().at_least = 4;
  cg.option().auto_bin_max = 32;
  cg.option().detect_overlap = false;
  cg.option().cross_num_print_missing = 3;

  EXPECT_EQ(cg.cvp_a.option().at_least, 4u);
  EXPECT_EQ(cg.cvp_a.option().auto_bin_max, 32u);
  EXPECT_FALSE(cg.cvp_a.option().detect_overlap);
  EXPECT_EQ(cg.crs.option().at_least, 4u);
  EXPECT_EQ(cg.crs.option().cross_num_print_missing, 3u);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvg_option_inheritance, at_least_coverage_and_merge_instances) {
  auto cntxt = fc4sc::global::create_new_context();
  option_inh_cg cg1("cg1", __FILE__, __LINE__, cntxt);
  option_inh_cg cg2("cg2", __FILE__, __LINE__, cntxt);
  cg1.type_option().merge_instances = true;
  cg1.option().at_least = 2;
  cg2.option().at_least = 2;

  cg1.sample_vals(1, 1);
  cg1.sample_vals(7, 2);

  EXPECT_EQ(cg1.cvp_a.get_inst_coverage(), 0);
  EXPECT_EQ(cg1.cvp_b.get_inst_coverage(), 0);
  EXPECT_EQ(cg1.crs.get_inst_coverage(), 0);
  EXPECT_EQ(cg1.get_inst_coverage(), 0);
  EXPECT_EQ(cg1.get_coverage(), 0);

  cg2.sample_vals(1, 1);
  // Merged counts for (low, b1) / <0,0> reach 2 -> 1 bin covered in each
  EXPECT_DOUBLE_EQ(cg1.cvp_a.get_coverage(), 50.0);
  EXPECT_DOUBLE_EQ(cg1.cvp_b.get_coverage(), 50.0);
  EXPECT_DOUBLE_EQ(cg1.crs.get_coverage(), 25.0);

  // Second hit in cg1 brings cg1 instance coverage up too
  cg1.sample_vals(1, 1);
  EXPECT_DOUBLE_EQ(cg1.cvp_a.get_inst_coverage(), 50.0);
  EXPECT_DOUBLE_EQ(cg1.cvp_b.get_inst_coverage(), 50.0);
  EXPECT_DOUBLE_EQ(cg1.crs.get_inst_coverage(), 25.0);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvg_option_inheritance, detect_overlap_and_child_override) {
  auto cntxt = fc4sc::global::create_new_context();
  option_inh_cg cg("cg", __FILE__, __LINE__, cntxt);

  // Set after initial sample; first subsequent sample warns once
  ::testing::internal::CaptureStderr();
  cg.sample_vals(1, 1);
  EXPECT_EQ(::testing::internal::GetCapturedStderr().find("overlap"), std::string::npos);

  cg.option().detect_overlap = true;
  ::testing::internal::CaptureStderr();
  cg.sample_vals(1, 1);
  cg.sample_vals(4, 2);
  std::string err = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(err.find("Warning"), std::string::npos) << err;
  EXPECT_EQ(err.find("Warning"), err.rfind("Warning")) << err;

  // Stopped covergroup still checks detect_overlap on sample when inherited
  option_inh_cg cg_stopped("cg_stopped", __FILE__, __LINE__, cntxt);
  cg_stopped.option().detect_overlap = true;
  cg_stopped.stop();
  ::testing::internal::CaptureStderr();
  cg_stopped.sample_vals(4, 1);
  std::string stopped_err = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(stopped_err.find("overlap"), std::string::npos) << stopped_err;

  // Child override false after cg.option().detect_overlap = true suppresses warning
  option_inh_cg cg_suppressed("cg_suppressed", __FILE__, __LINE__, cntxt);
  cg_suppressed.option().detect_overlap = true;
  cg_suppressed.cvp_a.option().detect_overlap = false;
  ::testing::internal::CaptureStderr();
  cg_suppressed.sample_vals(4, 1);
  EXPECT_EQ(::testing::internal::GetCapturedStderr().find("overlap"), std::string::npos);
  EXPECT_FALSE(cg_suppressed.cvp_a.option().detect_overlap);
  EXPECT_TRUE(cg_suppressed.cvp_b.option().detect_overlap);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvg_option_inheritance, overrides_and_per_field_independence) {
  auto cntxt = fc4sc::global::create_new_context();
  option_inh_cg cg("cg", __FILE__, __LINE__, cntxt);

  // Child override BEFORE cg.option() (non-default)
  cg.cvp_a.option().at_least = 5;
  // Child override AFTER cg.option() (even overriding back to 1)
  cg.option().at_least = 3;
  cg.option().auto_bin_max = 20;
  cg.option().detect_overlap = true;
  cg.option().cross_num_print_missing = 2;

  EXPECT_EQ(cg.cvp_a.option().at_least, 5u);
  EXPECT_EQ(cg.cvp_b.option().at_least, 3u);
  EXPECT_EQ(cg.crs.option().at_least, 3u);

  // Per-field independence: overriding at_least on cvp_a does not block auto_bin_max or detect_overlap
  EXPECT_EQ(cg.cvp_a.option().auto_bin_max, 20u);
  EXPECT_TRUE(cg.cvp_a.option().detect_overlap);

  // Override cvp_b back to default 1 after cg.option().at_least = 3
  cg.cvp_b.option().at_least = 1;
  cg.crs.option().at_least = 1;
  // Re-assign cg.option().at_least -> overridden children keep their values
  cg.option().at_least = 4;
  EXPECT_EQ(cg.cvp_a.option().at_least, 5u);
  EXPECT_EQ(cg.cvp_b.option().at_least, 1u);
  EXPECT_EQ(cg.crs.option().at_least, 1u);
  EXPECT_EQ(cg.crs.option().cross_num_print_missing, 2u);

  fc4sc::global::delete_context(cntxt);
}

TEST(cvg_option_inheritance, xml_export_all_options_and_cross_print_missing) {
  auto cntxt = fc4sc::global::create_new_context();
  option_inh_cg cg("cg", __FILE__, __LINE__, cntxt);
  cg.option().at_least = 2;
  cg.option().auto_bin_max = 16;
  cg.option().detect_overlap = true;
  cg.option().cross_num_print_missing = 2;

  cg.sample_vals(1, 1); // hits 1 cross bin out of 4 (3 missing)

  const std::string xml_file = "cvg_option_inheritance_xml.xml";
  xml_printer::coverage_save(xml_file, cntxt);

  auto root = fc4sc::ucis_db::read_xml_file(xml_file);
  const xml_node* cvp_a = find_node_by_name(*root, "coverpoint", "cvp_a");
  ASSERT_NE(cvp_a, nullptr);
  const xml_node* cvp_a_opts = cvp_a->element("options");
  ASSERT_NE(cvp_a_opts, nullptr);
  EXPECT_EQ(cvp_a_opts->get("at_least"), "2");
  EXPECT_EQ(cvp_a_opts->get("auto_bin_max"), "16");
  EXPECT_EQ(cvp_a_opts->get("detect_overlap"), "1");
  EXPECT_EQ(cvp_a_opts->get("cross_num_print_missing"), "");

  const xml_node* crs = find_node_by_name(*root, "cross", "crs");
  ASSERT_NE(crs, nullptr);
  const xml_node* crs_opts = crs->element("options");
  ASSERT_NE(crs_opts, nullptr);
  EXPECT_EQ(crs_opts->get("at_least"), "2");
  EXPECT_EQ(crs_opts->get("cross_num_print_missing"), "2");
  EXPECT_EQ(crs_opts->get("auto_bin_max"), "");
  EXPECT_EQ(crs_opts->get("detect_overlap"), "");

#ifndef FC4SC_SAVE_ALL_CROSS_BINS
  // 1 hit crossBin + 2 missing crossBins = 3
  EXPECT_EQ(crs->elements("crossBin").size(), 3u);
#endif

  fc4sc::global::delete_context(cntxt);
}

TEST(cvg_option_inheritance, dynamic_covergroup_inheritance) {
  auto cntxt = fc4sc::global::create_new_context();
  int x = 0;
  int y = 0;

  fc4sc::dynamic_covergroup_factory type("dyn_opt_cg");
  auto cvp_x_fact = type.create_coverpoint<int(int)>("cvp_x", [](int v) { return v; });
  cvp_x_fact.create_bin("x0", 0);
  cvp_x_fact.create_bin("x1", 1);

  auto cvp_y_fact = type.create_coverpoint<int(int)>("cvp_y", [](int v) { return v; });
  cvp_y_fact.create_bin("y0", 0);
  cvp_y_fact.create_bin("y1", 1);

  type.add_cross("crs_xy", cvp_x_fact, cvp_y_fact);

  fc4sc::dynamic_covergroup inst(type, "inst", __FILE__, __LINE__, cntxt);
  cvp_x_fact.bind_sample(inst, x);
  cvp_y_fact.bind_sample(inst, y);
  inst.option().at_least = 2;
  inst.option().auto_bin_max = 42;
  inst.option().detect_overlap = true;
  inst.option().cross_num_print_missing = 1;

  auto& cvp_x = static_cast<fc4sc::coverpoint_base&>(inst.get_coverpoint("cvp_x"));
  auto& crs_xy = static_cast<fc4sc::cross_base&>(inst.get_coverpoint("crs_xy"));

  EXPECT_EQ(cvp_x.option().at_least, 2u);
  EXPECT_EQ(cvp_x.option().auto_bin_max, 42u);
  EXPECT_TRUE(cvp_x.option().detect_overlap);
  EXPECT_EQ(crs_xy.option().at_least, 2u);
  EXPECT_EQ(crs_xy.option().cross_num_print_missing, 1u);

  x = 0; y = 0; inst.sample();
  EXPECT_EQ(inst.get_inst_coverage(), 0);
  x = 0; y = 0; inst.sample();
  // cvp_x: 1/2 = 50%, cvp_y: 1/2 = 50%, crs_xy: 1/4 = 25% -> avg (50+50+25)/3
  EXPECT_NEAR(inst.get_inst_coverage(), (50.0 + 50.0 + 25.0) / 3.0, 1e-6);

  const std::string xml_file = "cvg_option_inheritance_dynamic.xml";
  xml_printer::coverage_save(xml_file, cntxt);
  auto root = fc4sc::ucis_db::read_xml_file(xml_file);
  const xml_node* crs_node = find_node_by_name(*root, "cross", "crs_xy");
  ASSERT_NE(crs_node, nullptr);
  EXPECT_EQ(crs_node->element("options")->get("at_least"), "2");
  EXPECT_EQ(crs_node->element("options")->get("cross_num_print_missing"), "1");
#ifndef FC4SC_SAVE_ALL_CROSS_BINS
  EXPECT_EQ(crs_node->elements("crossBin").size(), 2u);
#endif

  fc4sc::global::delete_context(cntxt);
}
