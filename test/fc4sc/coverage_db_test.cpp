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
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

using fc4sc::ucis_db::xml_node;
using fc4sc::ucis_db::xml_parser;

static std::unique_ptr<xml_node> parse(const std::string& text) {
  return xml_parser(text, "test").parse();
}

TEST(coverage_db, xml_reader) {
  auto root = parse("\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<!-- comment -->\n"
                    "<a x='1' y = \"&lt;&amp;&gt;&quot;&apos;&#65;&#x42;&#xe9;\">"
                    "<b/><!-- inner --><c>t&#10;<![CDATA[<raw>]]></c><?pi x?><b k=\"2\"></b></a>\n<!-- end -->");
  EXPECT_EQ(root->name, "a");
  EXPECT_EQ(root->get("x"), "1");
  EXPECT_EQ(root->get("y"), "<&>\"'AB\xC3\xA9");
  ASSERT_EQ(root->children.size(), 3u);
  EXPECT_EQ(root->elements("b").size(), 2u);
  EXPECT_EQ(root->elements("b")[1]->get("k"), "2");
  EXPECT_EQ(root->element("c")->text, "t\n<raw>");
  EXPECT_EQ(root->element("c")->line, 3u);

  // written and read again
  std::stringstream out;
  fc4sc::ucis_db::write_xml(*root, out);
  auto again = parse(out.str());
  EXPECT_EQ(again->get("y"), root->get("y"));
  EXPECT_EQ(again->element("c")->text, root->element("c")->text);

  const char* bad[] = {
    "", "text", "<a>", "<a></b>", "<a x='1' x='2'/>", "<a x=1/>", "<a x='<'/>",
    "<a>&unknown;</a>", "<a>&#0;</a>", "<a>&#x110000;</a>", "<a/><b/>", "<a/>text",
    "<!DOCTYPE a><a/>", "<a><!-- </a>", "<a><![CDATA[x</a>", "<a x='1'y='2'/>", "<1a/>"
  };
  for (auto text : bad)
    EXPECT_THROW(parse(text), std::runtime_error) << text;

  std::string deep;
  for (int i = 0; i < 300; ++i) deep += "<a>";
  for (int i = 0; i < 300; ++i) deep += "</a>";
  EXPECT_THROW(parse(deep), std::runtime_error);
  try {
    parse("<a>\n<b>\n</a>");
    FAIL();
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("test:3:"), std::string::npos) << e.what();
  }
  EXPECT_THROW(fc4sc::ucis_db::read_xml_file("no_such_file.xml"), std::runtime_error);
}

class db_child_scope : public fc4sc::scope {
public:
  SCOPE_DECL(db_child_scope)

  class db_cvg : public covergroup {
  public:
    CG_SCOPED_CONS(db_cvg, db_child_scope) { }
    int x = 0;
    COVERPOINT(int, cvp, x) { bin<int>("low", interval(0, 3)), bin<int>("high", interval(4, 7)) };
  };

  db_child_scope(fc4sc::scp_base& parent, std::string name, const char* file = "", int line = 1) :
    fc4sc::scope(parent, name, __FILE__, __LINE__, file, line), CG_SCOPED_INST(child_cvg) { }

  db_cvg child_cvg;
};

class db_top_scope : public fc4sc::scope {
public:
  SCOPE_DECL(db_top_scope)

  db_top_scope(std::string name, fc4sc::global* cntxt) :
    fc4sc::scope(name, __FILE__, __LINE__, "", 1, cntxt), sub(*this, "sub", __FILE__, __LINE__) { }

  db_child_scope sub;
};

// a covergroup with every kind of bin
class db_cvg : public covergroup {
public:
  CG_CONS(db_cvg) {};

  int x = 0;
  int y = 0;

  COVERPOINT(int, values, x) {
    bin<int>("zero", 0),
    bin<int>("odd", 1, 3, 5),
    bin_array<int>("arr", 3, interval(6, 11)),
    transition_bin<int>("up", fc4sc::trans<int>(1, 2), fc4sc::trans<int>(2, 3)),
    ignore_bin<int>("ign", 13),
    illegal_bin<int>("bad", 99),
    default_bin<int>("others")
  };

  COVERPOINT(int, each, x) {
    bin<int>("small", interval(0, 4)),
    default_bin_array<int>("rest")
  };

  COVERPOINT(int, kind, y) {
    bin<int>("a", 0),
    bin<int>("b", 1)
  };

  cross<int, int> auto_cross = cross<int, int>(this, "auto_cross", &values, &kind);
  cross<int, int> user_cross = cross<int, int>(this, "user_cross", &each, &kind,
    fc4sc::cross_bin("small_a", binsof<int>(&each) && binsof<int>(&kind, "a")));
};

struct run_values {
  std::vector<std::pair<int, int>> samples;
};

static run_values make_run(unsigned seed, int count) {
  std::mt19937 gen(seed);
  // no 99, which is illegal
  std::uniform_int_distribution<int> x(0, 20), y(0, 1);
  run_values run;
  for (int i = 0; i < count; ++i)
    run.samples.push_back(std::make_pair(x(gen), y(gen)));
  return run;
}

// The model of a test: two instances in the default scope, and a scope
// with a child scope
struct db_model {
  fc4sc::global* cntxt;
  std::unique_ptr<db_cvg> first, second;
  std::unique_ptr<db_top_scope> top;

  explicit db_model(bool with_second = true) : cntxt(fc4sc::global::create_new_context()) {
    first.reset(new db_cvg("first", __FILE__, __LINE__, cntxt));
    if (with_second) second.reset(new db_cvg("second", __FILE__, __LINE__, cntxt));
    top.reset(new db_top_scope("top", cntxt));
  }

  ~db_model() {
    first.reset();
    second.reset();
    top.reset();
    fc4sc::global::delete_context(cntxt);
  }

  void sample(const run_values& run) {
    for (auto &s : run.samples) {
      first->x = s.first;
      first->y = s.second;
      first->sample();
      if (second) {
        second->x = s.first / 2;
        second->y = 1 - s.second;
        second->sample();
      }
      top->sub.child_cvg.x = s.first % 8;
      top->sub.child_cvg.sample();
    }
  }

  void save(const std::string& file) { xml_printer::coverage_save(file, cntxt); }
};

// The coverage counts of a UCIS XML file which are not zero, by the path of
// their element
typedef std::map<std::string, uint64_t> counts_t;

static void collect(const xml_node& node, const std::string& path, counts_t& counts) {
  std::string here = path + "/" + node.name;
  if (node.attribute("name")) here += "[" + node.get("name") + "]";
  if (node.attribute("type")) here += "{" + node.get("type") + "}";
  if (node.name == "range") here += "[" + node.get("from") + ":" + node.get("to") + "]";
  if (node.name == "sequence")
    for (auto v : node.elements("seqValue")) here += "," + v->text;
  if (node.name == "range" || node.name == "sequence" || node.name == "crossBin") {
    uint64_t count = fc4sc::ucis_db::coverage_count(node);
    if (count != 0) counts[here] += count;
    return;
  }
  for (auto &child : node.children)
    collect(*child, here, counts);
}

static counts_t counts_of(const std::string& file) {
  counts_t counts;
  collect(*fc4sc::ucis_db::read_xml_file(file), "", counts);
  return counts;
}

TEST(coverage_db, merge_matches_a_single_run) {
  const run_values run1 = make_run(1, 60), run2 = make_run(2, 60), run3 = make_run(3, 60);
  { db_model m; m.sample(run1); m.save("coverage_db_run1.xml"); }
  { db_model m; m.sample(run2); m.save("coverage_db_run2.xml"); }
  { db_model m; m.sample(run3); m.save("coverage_db_run3.xml"); }
  { db_model m; m.sample(run1); m.sample(run2); m.sample(run3); m.save("coverage_db_all.xml"); }

  fc4sc::merge_coverage_db({"coverage_db_run1.xml", "coverage_db_run2.xml", "coverage_db_run3.xml"},
                           "coverage_db_merged.xml");
  const counts_t merged = counts_of("coverage_db_merged.xml");
  const counts_t all = counts_of("coverage_db_all.xml");
  EXPECT_GT(all.size(), 40u);
  EXPECT_EQ(merged, all);

  // the merged file can be merged again
  fc4sc::merge_coverage_db({"coverage_db_merged.xml"}, "coverage_db_merged_copy.xml");
  EXPECT_EQ(counts_of("coverage_db_merged_copy.xml"), all);
}

TEST(coverage_db, merge_files_with_other_instances) {
  const run_values run1 = make_run(4, 30), run2 = make_run(5, 30);
  { db_model m(false); m.sample(run1); m.save("coverage_db_one.xml"); }
  { db_model m; m.sample(run2); m.save("coverage_db_two.xml"); }
  fc4sc::merge_coverage_db({"coverage_db_one.xml", "coverage_db_two.xml"}, "coverage_db_one_two.xml");

  counts_t expected = counts_of("coverage_db_one.xml");
  for (auto &c : counts_of("coverage_db_two.xml")) expected[c.first] += c.second;
  EXPECT_EQ(counts_of("coverage_db_one_two.xml"), expected);

  // the instance "second" of the second file is added to the first
  auto root = fc4sc::ucis_db::read_xml_file("coverage_db_one_two.xml");
  int instances = 0;
  std::map<std::string, int> keys;
  for (auto inst : root->elements("instanceCoverages")) {
    keys[inst->get("instanceId")]++;
    for (auto group : inst->elements("covergroupCoverage"))
      instances += group->elements("cgInstance").size();
  }
  // first and the child scope, and second of the second file
  EXPECT_EQ(instances, 3);
  for (auto &k : keys) EXPECT_EQ(k.second, 1) << "instanceId " << k.first;
  EXPECT_EQ(root->elements("historyNodes").size(), 2u);
}

TEST(coverage_db, merge_files_of_other_scopes) {
  // the files of the merge test have different scopes and covergroups
  const char* scopes = R"(<?xml version="1.0" encoding="UTF-8"?>
<UCIS xmlns="UCIS" ucisVersion="1.0" writtenBy="t" writtenTime="2026-01-01T00:00:00">
<sourceFiles fileName="a.cpp" id="1"/>
<sourceFiles fileName="b.cpp" id="2"/>
<historyNodes historyNodeId="200" logicalName="s" testStatus="true" date="2026-01-01T00:00:00" toolCategory="s" ucisVersion="1.0" vendorId="s" vendorTool="s" vendorToolVersion="s"/>
<instanceCoverages name="top" moduleName="top_t" key="1" instanceId="3">
<id file="2" line="1" inlineCount="1"/>
</instanceCoverages>
<instanceCoverages name="sub" moduleName="sub_t" key="2" instanceId="4" parentInstanceId="3">
<id file="1" line="1" inlineCount="1"/>
<covergroupCoverage weight="1">
<cgInstance name="cg" key="3">
<options/>
<cgId cgName="cg_t" moduleName="sub_t">
<cginstSourceId file="1" line="1" inlineCount="1"/>
<cgSourceId file="2" line="1" inlineCount="1"/>
</cgId>
<coverpoint name="cvp" key="4">
<options/>
<coverpointBin name="b" key="5" type="default">
<range from="0" to="0"><contents coverageCount="3"/></range>
</coverpointBin>
</coverpoint>
</cgInstance>
</covergroupCoverage>
</instanceCoverages>
</UCIS>
)";
  {
    std::ofstream f("coverage_db_scopes.xml");
    f << scopes;
  }
  // the second file has the child scope only, with its source file first
  std::string other = scopes;
  const std::string a_cpp = "fileName=\"a.cpp\"";
  other.replace(other.find(a_cpp), a_cpp.size(), "fileName=\"c.cpp\"");
  {
    std::ofstream f("coverage_db_scopes2.xml");
    f << other;
  }
  fc4sc::merge_coverage_db({"coverage_db_scopes.xml", "coverage_db_scopes2.xml"}, "coverage_db_scopes_merged.xml");
  auto root = fc4sc::ucis_db::read_xml_file("coverage_db_scopes_merged.xml");
  // c.cpp is added as file 3, and the other files keep their ids
  ASSERT_EQ(root->elements("sourceFiles").size(), 3u);
  EXPECT_EQ(root->elements("sourceFiles")[2]->get("fileName"), "c.cpp");
  EXPECT_EQ(root->elements("sourceFiles")[2]->get("id"), "3");
  // the scopes are matched by their path: the counts are added
  ASSERT_EQ(root->elements("instanceCoverages").size(), 2u);
  EXPECT_EQ(counts_of("coverage_db_scopes_merged.xml").begin()->second, 6u);
}

TEST(coverage_db, load_coverage_db) {
  const run_values run1 = make_run(6, 60), run2 = make_run(7, 60);
  double expected_coverage = 0, expected_cross = 0;
  { db_model m; m.sample(run1); m.save("coverage_db_load1.xml"); }
  {
    db_model m;
    m.sample(run1);
    m.sample(run2);
    m.save("coverage_db_load_all.xml");
    expected_coverage = m.first->get_inst_coverage();
    expected_cross = m.first->user_cross.get_inst_coverage();
  }
  {
    db_model m;
    const uint64_t loaded = fc4sc::load_coverage_db("coverage_db_load1.xml", m.cntxt);
    EXPECT_GT(loaded, 40u);
    m.sample(run2);
    m.save("coverage_db_loaded.xml");
    EXPECT_EQ(m.first->get_inst_coverage(), expected_coverage);
    EXPECT_EQ(m.first->user_cross.get_inst_coverage(), expected_cross);
  }
  EXPECT_EQ(counts_of("coverage_db_loaded.xml"), counts_of("coverage_db_load_all.xml"));
}

TEST(coverage_db, load_skips_what_is_not_in_the_model) {
  const run_values run = make_run(8, 40);
  { db_model m; m.sample(run); m.save("coverage_db_skip.xml"); }
  db_model m(false);
  fc4sc::ucis_db::loader loader;
  loader.load(*fc4sc::ucis_db::read_xml_file("coverage_db_skip.xml"), m.cntxt);
  // the covergroup "second" is not in the model
  EXPECT_EQ(loader.not_found, 1u);
  EXPECT_GT(loader.loaded, 20u);
}

TEST(coverage_db, set_coverage_db_name) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    db_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample();
    EXPECT_EQ(fc4sc::global::get_coverage_db_name(cntxt), "");
    fc4sc::global::set_coverage_db_name("coverage_db_named.xml", cntxt);
    EXPECT_EQ(fc4sc::global::get_coverage_db_name(cntxt), "coverage_db_named.xml");
    xml_printer::coverage_save("", cntxt);
  }
  fc4sc::global::delete_context(cntxt);
  EXPECT_FALSE(counts_of("coverage_db_named.xml").empty());
}

TEST(coverage_db, bad_counts) {
  const char* file = R"(<UCIS ucisVersion="1.0" writtenBy="t" writtenTime="2026-01-01T00:00:00">
<instanceCoverages name="top" moduleName="t" key="1" instanceId="1">
<covergroupCoverage><cgInstance name="cg" key="2"><cgId cgName="c"/>
<coverpoint name="cvp" key="3"><coverpointBin name="b" key="4" type="default">
<range from="0" to="0"><contents coverageCount="-1"/></range>
</coverpointBin></coverpoint></cgInstance></covergroupCoverage>
</instanceCoverages></UCIS>
)";
  {
    std::ofstream f("coverage_db_bad.xml");
    f << file;
  }
  // the count is read when it is added
  EXPECT_THROW(fc4sc::merge_coverage_db({"coverage_db_bad.xml", "coverage_db_bad.xml"}, "coverage_db_bad_merged.xml"),
               std::runtime_error);
  EXPECT_THROW(fc4sc::merge_coverage_db({}, "coverage_db_none.xml"), std::runtime_error);
  uint64_t v = 0;
  EXPECT_TRUE(fc4sc::ucis_db::parse_uint("18446744073709551615", v));
  EXPECT_EQ(v, 18446744073709551615ull);
  EXPECT_FALSE(fc4sc::ucis_db::parse_uint("18446744073709551616", v));
  EXPECT_EQ(fc4sc::ucis_db::add_saturated(18446744073709551615ull, 2), 18446744073709551615ull);
}

TEST(coverage_db, load_a_run_without_samples) {
  { db_model m; m.save("coverage_db_no_samples.xml"); }
  db_model m;
  fc4sc::load_coverage_db("coverage_db_no_samples.xml", m.cntxt);
  // a cross bin which no run hit is not added to the hit cross bins
  auto crs = static_cast<fc4sc::cross_base_data_model*>(m.first->auto_cross.get_data());
  EXPECT_TRUE(crs->get_cross_bins().empty());
  crs = static_cast<fc4sc::cross_base_data_model*>(m.first->user_cross.get_data());
  EXPECT_TRUE(crs->get_cross_bins().empty());
}
