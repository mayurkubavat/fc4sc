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
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

typedef std::vector<std::vector<int>> sequences_t;

static void sample_values(fc4sc::dynamic_covergroup& cvg, int& var, std::initializer_list<int> values) {
  for (int v : values) {
    var = v;
    cvg.sample();
  }
}

static fc4sc::coverpoint_base& get_cvp(fc4sc::dynamic_covergroup& cvg, const std::string& name) {
  return dynamic_cast<fc4sc::coverpoint_base&>(cvg.get_coverpoint(name));
}

static uint64_t hits(fc4sc::dynamic_covergroup& cvg, const std::string& cvp, size_t bin) {
  return get_cvp(cvg, cvp).get_bins_base().at(bin)->get_hitcount();
}

// SystemVerilog:
//   coverpoint x {
//     bins one = {1};
//     bins up = (1 => 2 => 3), (5 => 6);
//     bins arr[] = (1, 2 => 4);
//     ignore_bins ign = (0 => 1 => 2);
//     illegal_bins bad = (9 => 9);
//   }
static void define_type(fc4sc::dynamic_covergroup_factory& type) {
  auto x = type.create_coverpoint<int(int)>("x", [](int v) { return v; });
  x.create_bin("one", 1);
  x.create_transition_bin("up", trans<int>(1, 2, 3), trans<int>(5, 6));
  x.create_transition_bin_array("arr", trans<int>(std::vector<int>{1, 2}, 4));
  x.create_ignore_transition_bin("ign", trans<int>(0, 1, 2));
  x.create_illegal_transition_bin("bad", trans<int>(9, 9));
}

TEST(dynamic_transition_bin, bins_are_created) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory type("dyn_trans");
    define_type(type);
    fc4sc::dynamic_covergroup cvg(type, "cvg", __FILE__, __LINE__, cntxt);
    auto& x = get_cvp(cvg, "x");
    // one, up, arr[0], arr[1]
    ASSERT_EQ(x.size(), 4u);
    EXPECT_EQ(x.get_bins_base()[1]->name(), "up");
    EXPECT_EQ(x.get_bins_base()[2]->name(), "arr[0]");
    EXPECT_EQ(x.get_bins_base()[3]->name(), "arr[1]");
    auto data = dynamic_cast<fc4sc::coverpoint_base_data_model*>(x.get_data());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->bins_data[1]->get_sequences_to_int(), (sequences_t{{1, 2, 3}, {5, 6}}));
    EXPECT_EQ(data->bins_data[3]->get_sequences_to_int(), (sequences_t{{2, 4}}));
    EXPECT_EQ(data->ignore_bins_data[0]->get_sequences_to_int(), (sequences_t{{0, 1, 2}}));
    EXPECT_EQ(data->illegal_bins_data[0]->get_sequences_to_int(), (sequences_t{{9, 9}}));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(dynamic_transition_bin, sampling) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory type("dyn_trans");
    auto x = type.create_coverpoint<int(int)>("x", [](int v) { return v; });
    x.create_bin("one", 1);
    x.create_transition_bin("up", trans<int>(1, 2, 3), trans<int>(5, 6));
    x.create_transition_bin_array("arr", trans<int>(std::vector<int>{1, 2}, 4));
    x.create_ignore_transition_bin("ign", trans<int>(0, 1, 2));
    x.create_illegal_transition_bin("bad", trans<int>(9, 9));

    int v = 0;
    fc4sc::dynamic_covergroup cvg(type, "cvg", __FILE__, __LINE__, cntxt);
    x.bind_sample(cvg, v);

    sample_values(cvg, v, {1, 2, 3});
    EXPECT_EQ(hits(cvg, "x", 0), 1u);
    EXPECT_EQ(hits(cvg, "x", 1), 1u);
    sample_values(cvg, v, {5, 6, 2, 4});
    EXPECT_EQ(hits(cvg, "x", 1), 2u);
    EXPECT_EQ(hits(cvg, "x", 2), 0u);
    EXPECT_EQ(hits(cvg, "x", 3), 1u);
    // 1 => 2 => 3 inside the ignored 0 => 1 => 2 => 3 is still hit: the
    // ignored sequence is 0 => 1 => 2 only
    sample_values(cvg, v, {1, 4});
    EXPECT_EQ(hits(cvg, "x", 2), 1u);
    EXPECT_EQ(get_cvp(cvg, "x").get_inst_coverage(), 100);

    v = 9;
    cvg.sample();
    EXPECT_THROW(cvg.sample(), fc4sc::illegal_bin_sample_exception);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(dynamic_transition_bin, instances_have_their_own_history) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory type("dyn_trans");
    auto x = type.create_coverpoint<int(int)>("x", [](int v) { return v; });
    x.create_transition_bin("up", trans<int>(1, 2));

    int a = 0, b = 0;
    fc4sc::dynamic_covergroup cvg_a(type, "a", __FILE__, __LINE__, cntxt);
    fc4sc::dynamic_covergroup cvg_b(type, "b", __FILE__, __LINE__, cntxt);
    x.bind_sample(cvg_a, a);
    x.bind_sample(cvg_b, b);

    // 1 in a then 2 in b is no transition of either instance
    sample_values(cvg_a, a, {1});
    sample_values(cvg_b, b, {2});
    EXPECT_EQ(hits(cvg_a, "x", 0), 0u);
    EXPECT_EQ(hits(cvg_b, "x", 0), 0u);
    sample_values(cvg_a, a, {2});
    EXPECT_EQ(hits(cvg_a, "x", 0), 1u);
    EXPECT_EQ(hits(cvg_b, "x", 0), 0u);
    EXPECT_EQ(get_cvp(cvg_a, "x").get_inst_coverage(), 100);
    EXPECT_EQ(get_cvp(cvg_b, "x").get_inst_coverage(), 0);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(dynamic_transition_bin, cross) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory type("dyn_trans");
    auto x = type.create_coverpoint<int(int)>("x", [](int v) { return v; });
    x.create_transition_bin("up", trans<int>(1, 2));
    auto y = type.create_coverpoint<int(int)>("y", [](int v) { return v; });
    y.create_bin("seven", 7);
    type.add_cross("x_y", x, y);

    int vx = 0, vy = 7;
    fc4sc::dynamic_covergroup cvg(type, "cvg", __FILE__, __LINE__, cntxt);
    x.bind_sample(cvg, vx);
    y.bind_sample(cvg, vy);
    sample_values(cvg, vx, {1, 2});
    auto& crs = dynamic_cast<fc4sc::cross_base&>(cvg.get_coverpoint("x_y"));
    EXPECT_EQ(crs.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(dynamic_transition_bin, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    fc4sc::dynamic_covergroup_factory type("dyn_trans");
    auto x = type.create_coverpoint<int(int)>("x", [](int v) { return v; });
    x.create_transition_bin("up", trans<int>(1, 2, 3), trans<int>(5, 6));
    int v = 0;
    fc4sc::dynamic_covergroup cvg(type, "cvg", __FILE__, __LINE__, cntxt);
    x.bind_sample(cvg, v);
    sample_values(cvg, v, {5, 6});
    xml_printer::coverage_save("dynamic_transition_bin_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("dynamic_transition_bin_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();
  size_t pos = xml.find("<coverpointBin name=\"up\"");
  ASSERT_NE(pos, std::string::npos);
  const std::string up = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(up.find("<sequence>\n<contents coverageCount=\"0\"></contents>\n"
                    "<seqValue>1</seqValue>\n<seqValue>2</seqValue>\n<seqValue>3</seqValue>"),
            std::string::npos);
  EXPECT_NE(up.find("<sequence>\n<contents coverageCount=\"1\"></contents>\n"
                    "<seqValue>5</seqValue>\n<seqValue>6</seqValue>"),
            std::string::npos);
}
