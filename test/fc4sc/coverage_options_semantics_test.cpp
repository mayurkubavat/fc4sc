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
#include <map>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

using fc4sc::ucis_db::xml_node;

// The bins of coverpoint a are given to the constructor, so that instances
// of the type can have different bins
class sv_opt_cvg : public covergroup {
public:
  std::string n0;
  int v0;
  std::string n1;
  int v1;

  CG_CONS(sv_opt_cvg, std::string name0 = "lo", int value0 = 0, std::string name1 = "hi", int value1 = 1),
    n0(name0), v0(value0), n1(name1), v1(value1) {}

  int x = 0;
  int y = 0;

  void sample(int x, int y) {
    this->x = x;
    this->y = y;
    covergroup::sample();
  }

  COVERPOINT(int, a, x) { bin<int>(n0, v0), bin<int>(n1, v1) };
  COVERPOINT(int, b, y) { bin<int>("y0", 0), bin<int>("y1", 1) };
  cross<int, int> a_b = cross<int, int>(this, "a_b", &a, &b);
};

static double near(double v) { return static_cast<int>(v * 1000 + 0.5) / 1000.0; }

static const xml_node* covergroup_coverage(const xml_node& root) {
  for (auto inst : root.elements("instanceCoverages"))
    for (auto group : inst->elements("covergroupCoverage"))
      return group;
  return nullptr;
}

static std::vector<std::string> instance_names(const xml_node& group) {
  std::vector<std::string> names;
  for (auto cg : group.elements("cgInstance"))
    names.push_back(cg->get("name"));
  return names;
}

static const xml_node* child_named(const xml_node& node, const std::string& child, const std::string& name) {
  for (auto c : node.elements(child))
    if (c->get("name") == name) return c;
  return nullptr;
}

// the coverage counts of the bins of a coverpoint or cross, by bin name
static std::map<std::string, uint64_t> bin_counts(const xml_node& node) {
  std::map<std::string, uint64_t> counts;
  for (auto bin : node.elements("coverpointBin")) {
    uint64_t count = 0;
    for (auto range : bin->elements("range"))
      count += fc4sc::ucis_db::coverage_count(*range);
    counts[bin->get("name")] += count;
  }
  for (auto bin : node.elements("crossBin"))
    counts[bin->get("name")] += fc4sc::ucis_db::coverage_count(*bin);
  return counts;
}

static void sample_instances(sv_opt_cvg& inst1, sv_opt_cvg& inst2) {
  inst1.sample(1, 0);  // hi, y0
  inst2.sample(1, 1);  // hi, y1
  inst2.sample(2, 0);  // mid, y0
}

// Without strict_sv_options nothing changes: every instance is written,
// get_inst_coverage() is the instance coverage, and the options of the
// first instance are used for the merged coverage
TEST(coverage_options_semantics, default_is_unchanged) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt, "hi", 1, "mid", 2);
    EXPECT_FALSE(inst1.type_option().strict_sv_options);
    EXPECT_FALSE(inst1.option().per_instance);
    EXPECT_FALSE(inst1.option().get_inst_coverage);
    inst1.type_option().merge_instances = true;
    inst1.a.type_option().weight = 3;
    inst1.b.type_option().weight = 5;
    sample_instances(inst1, inst2);

    // a: 2 of 3 merged bins, b: 2 of 2, a_b: 3 of 6, weights 1
    EXPECT_EQ(near(inst1.get_coverage()), near((200.0 / 3 + 100 + 50) / 3));
    EXPECT_EQ(inst1.get_inst_coverage(), (50 + 50 + 25) / 3.0);
    xml_printer::coverage_save("coverage_options_default.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  auto root = fc4sc::ucis_db::read_xml_file("coverage_options_default.xml");
  EXPECT_EQ(instance_names(*covergroup_coverage(*root)), std::vector<std::string>({"inst1", "inst2"}));
}

// The type options of the coverpoints and crosses are shared by the
// instances of the type
TEST(coverage_options_semantics, type_option_is_shared) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.a.type_option().weight = 3;
    inst2.a_b.type_option().comment = "cross";
    EXPECT_EQ(inst2.a.type_option().weight, 3u);
    EXPECT_EQ(inst1.a_b.type_option().comment, "cross");
    EXPECT_EQ(inst1.b.type_option().weight, 1u);
    EXPECT_EQ(inst1.b.type_option().goal, 100u);
    EXPECT_EQ(&inst1.get_coverpoint("a").type_option(), &inst2.a.type_option());
  }
  fc4sc::global::delete_context(cntxt);
}

// 19.7.1: with merge_instances, the type coverage is the average of the
// merged coverage of the coverpoints and crosses weighted by their
// type_option.weight, and their type_option.goal is their target
TEST(coverage_options_semantics, merged_type_coverage_uses_type_option) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt, "hi", 1, "mid", 2);
    inst1.type_option().strict_sv_options = true;
    inst1.type_option().merge_instances = true;
    inst1.a.type_option().weight = 3;
    inst1.b.type_option().weight = 5;
    // instance weights, which are not used for the type coverage
    inst1.a.option().weight = 7;
    inst2.b.option().weight = 9;
    sample_instances(inst1, inst2);

    int covered = 0, total = 0;
    EXPECT_EQ(near(inst1.get_coverage(covered, total)), near((200.0 / 3 * 3 + 100 * 5 + 50) / 9));
    EXPECT_EQ(covered, 2 + 2 + 3);
    EXPECT_EQ(total, 3 + 2 + 6);
    EXPECT_EQ(near(inst1.a.get_coverage()), near(200.0 / 3));

    inst2.a.type_option().goal = 60;
    EXPECT_EQ(inst1.a.get_coverage(), 100);
    EXPECT_EQ(near(inst1.get_coverage()), near((100.0 * 3 + 100 * 5 + 50) / 9));
  }
  fc4sc::global::delete_context(cntxt);
}

// 19.11.3: without merge_instances, the type coverage of a coverpoint is its
// coverage in each instance weighted by its option.weight in that instance
TEST(coverage_options_semantics, weighted_cvp_type_coverage_uses_cvp_weights) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.a.option().weight = 3;
    inst1.sample(1, 0);  // a: 1 of 2
    inst2.sample(0, 0);
    inst2.sample(1, 0);  // a: 2 of 2

    // weights of the covergroup instances, 1 and 1
    EXPECT_EQ(inst1.a.get_coverage(), 75);
    inst1.type_option().strict_sv_options = true;
    EXPECT_EQ(inst1.a.get_coverage(), (50.0 * 3 + 100) / 4);
    inst1.a.type_option().goal = 60;
    EXPECT_EQ(inst1.a.get_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

// Table 19-1: get_inst_coverage only applies with merge_instances; when it
// is not set, get_inst_coverage() returns the value of get_coverage()
TEST(coverage_options_semantics, get_inst_coverage_option) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt, "hi", 1, "mid", 2);
    inst1.type_option().strict_sv_options = true;
    sample_instances(inst1, inst2);

    const double inst1_cov = (50 + 50 + 25) / 3.0;
    const double inst2_cov = (100 + 100 + 50) / 3.0;

    // without merge_instances, the instance coverage
    EXPECT_EQ(inst1.get_inst_coverage(), inst1_cov);

    inst1.type_option().merge_instances = true;
    const double type_cov = inst1.get_coverage();
    EXPECT_NE(type_cov, inst1_cov);
    EXPECT_EQ(inst1.get_inst_coverage(), type_cov);
    EXPECT_EQ(inst1.a.get_inst_coverage(), inst1.a.get_coverage());
    EXPECT_EQ(inst1.a_b.get_inst_coverage(), inst1.a_b.get_coverage());
    int covered = 0, total = 0, type_covered = 0, type_total = 0;
    EXPECT_EQ(inst1.get_inst_coverage(covered, total), inst1.get_coverage(type_covered, type_total));
    EXPECT_EQ(covered, type_covered);
    EXPECT_EQ(total, type_total);
    EXPECT_EQ(inst1.a_b.get_inst_coverage(covered, total), 50);
    EXPECT_EQ(covered, 3);
    EXPECT_EQ(total, 6);

    // the 19.11.3 example: tracked with get_inst_coverage
    inst2.option().get_inst_coverage = true;
    EXPECT_EQ(inst2.get_inst_coverage(), inst2_cov);
    EXPECT_EQ(inst2.a.get_inst_coverage(covered, total), 100);
    EXPECT_EQ(covered, 2);
    EXPECT_EQ(total, 2);
    EXPECT_EQ(inst1.get_inst_coverage(), type_cov);

    // without strict_sv_options, the instance coverage
    inst1.type_option().strict_sv_options = false;
    EXPECT_EQ(inst1.get_inst_coverage(), inst1_cov);
    EXPECT_EQ(inst1.a.get_inst_coverage(), 50);
  }
  fc4sc::global::delete_context(cntxt);
}

// Table 19-1: only the instances with per_instance are saved; the others
// contribute to the type coverage, written as one instance named after the
// type which is the union of their bins
TEST(coverage_options_semantics, per_instance_in_the_database) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt, "hi", 1, "mid", 2);
    sv_opt_cvg inst3("inst3", __FILE__, __LINE__, cntxt);
    inst1.type_option().strict_sv_options = true;
    inst1.type_option().comment = "type comment";
    inst1.type_option().weight = 2;
    inst3.option().per_instance = true;
    inst3.option().comment = "inst3 comment";
    inst1.option().comment = "inst1 comment";
    inst1.a.type_option().weight = 3;
    inst1.b.type_option().goal = 90;
    inst1.a_b.type_option().comment = "cross <type>";
    inst1.a.option().weight = 7;
    sample_instances(inst1, inst2);
    inst3.sample(0, 0);
    xml_printer::coverage_save("coverage_options_per_instance.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  auto root = fc4sc::ucis_db::read_xml_file("coverage_options_per_instance.xml");
  const xml_node* group = covergroup_coverage(*root);
  ASSERT_NE(group, nullptr);
  EXPECT_EQ(instance_names(*group), std::vector<std::string>({"sv_opt_cvg", "inst3"}));

  const xml_node* type = child_named(*group, "cgInstance", "sv_opt_cvg");
  ASSERT_NE(type, nullptr);
  EXPECT_EQ(type->element("options")->get("comment"), "type comment");
  EXPECT_EQ(type->element("options")->get("weight"), "2");
  EXPECT_EQ(type->element("options")->get("per_instance"), "false");
  EXPECT_EQ(child_named(*group, "cgInstance", "inst3")->element("options")->get("comment"), "inst3 comment");

  const xml_node* a = child_named(*type, "coverpoint", "a");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->element("options")->get("weight"), "3");
  std::map<std::string, uint64_t> a_bins = {{"lo", 0}, {"hi", 2}, {"mid", 1}};
  EXPECT_EQ(bin_counts(*a), a_bins);
  EXPECT_EQ(child_named(*type, "coverpoint", "b")->element("options")->get("goal"), "90");

  // the cross bins are matched by the names of the coverpoint bins: hi is
  // bin 1 of a in the type, but bin 0 in inst2
  const xml_node* a_b = child_named(*type, "cross", "a_b");
  ASSERT_NE(a_b, nullptr);
  EXPECT_EQ(a_b->element("options")->get("comment"), "cross <type>");
  std::map<std::string, uint64_t> a_b_bins = {{"<1,0>", 1}, {"<1,1>", 1}, {"<2,0>", 1}};
  EXPECT_EQ(bin_counts(*a_b), a_b_bins);
  for (auto bin : a_b->elements("crossBin")) {
    auto index = bin->elements("index");
    ASSERT_EQ(index.size(), 2u);
    EXPECT_EQ(bin->get("name"), "<" + index[0]->text + "," + index[1]->text + ">");
  }

  // the keys stay unique
  std::map<std::string, int> keys;
  std::vector<const xml_node*> todo(1, root.get());
  while (!todo.empty()) {
    const xml_node* n = todo.back();
    todo.pop_back();
    if (n->name != "userAttr" && n->attribute("key")) keys[n->get("key")]++;
    for (auto &c : n->children) todo.push_back(c.get());
  }
  for (auto &k : keys)
    EXPECT_EQ(k.second, 1) << k.first;
}

// The instance which holds the type coverage is loaded into the first
// instance without per_instance, and merged with the same instance of
// another file
TEST(coverage_options_semantics, type_instance_loaded_and_merged) {
  for (int run = 0; run < 2; ++run) {
    auto cntxt = fc4sc::global::create_new_context();
    {
      sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
      sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
      inst1.type_option().strict_sv_options = true;
      inst1.sample(0, 0);
      inst2.sample(0, 1);
      inst2.sample(1, 1);
      xml_printer::coverage_save("coverage_options_run" + std::to_string(run) + ".xml", cntxt);
    }
    fc4sc::global::delete_context(cntxt);
  }

  fc4sc::merge_coverage_db({"coverage_options_run0.xml", "coverage_options_run1.xml"},
                           "coverage_options_merged.xml");
  auto root = fc4sc::ucis_db::read_xml_file("coverage_options_merged.xml");
  const xml_node* group = covergroup_coverage(*root);
  ASSERT_NE(group, nullptr);
  EXPECT_EQ(instance_names(*group), std::vector<std::string>({"sv_opt_cvg"}));
  std::map<std::string, uint64_t> a_bins = {{"lo", 4}, {"hi", 2}};
  EXPECT_EQ(bin_counts(*child_named(*group->element("cgInstance"), "coverpoint", "a")), a_bins);

  auto cntxt = fc4sc::global::create_new_context();
  {
    sv_opt_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    sv_opt_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().strict_sv_options = true;
    // a: lo, hi; b: y0, y1; a_b: <0,0>, <0,1>, <1,1>
    EXPECT_EQ(fc4sc::load_coverage_db("coverage_options_merged.xml", cntxt), 7u);
    EXPECT_EQ(inst1.a.get_bin_hit_count(0), 4u);
    EXPECT_EQ(inst1.a.get_bin_hit_count(1), 2u);
    EXPECT_EQ(inst1.b.get_bin_hit_count(1), 4u);
    EXPECT_EQ(inst2.a.get_bin_hit_count(0), 0u);
  }
  fc4sc::global::delete_context(cntxt);
}
