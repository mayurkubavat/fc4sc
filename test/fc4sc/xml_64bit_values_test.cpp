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
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

using fc4sc::ucis_db::xml_node;

static const uint64_t u64_max = std::numeric_limits<uint64_t>::max();
static const int64_t i64_min = std::numeric_limits<int64_t>::min();

// Values which do not fit in an int: 64-bit values, and unsigned values
// above INT_MAX
class wide_values_cvg : public covergroup {
public:
  CG_CONS(wide_values_cvg) {};

  uint64_t u = 0;
  int64_t s = 0;
  uint32_t w = 0;

  COVERPOINT(uint64_t, u_cvp, u) {
    bin<uint64_t>("top", interval(uint64_t(0xFFFFFFFF00000000ULL), u64_max)),
    bin<uint64_t>("mid", uint64_t(0x100000000ULL)),
    transition_bin<uint64_t>("down", fc4sc::trans<uint64_t>(u64_max, uint64_t(0x80000000ULL)))
  };

  COVERPOINT(int64_t, s_cvp, s) {
    bin<int64_t>("min", i64_min),
    bin<int64_t>("neg", interval(int64_t(i64_min + 1), int64_t(-1)))
  };

  COVERPOINT(uint32_t, w_cvp, w) {
    bin<uint32_t>("high", interval(uint32_t(0x80000000u), uint32_t(0xFFFFFFFFu)))
  };

  void sample_values() {
    u = u64_max; s = i64_min; w = 0xFFFFFFFFu; sample();
    u = 0x80000000ULL; s = -5; w = 0x80000000u; sample();
    u = 0x100000000ULL; s = -1; sample();
  }
};

static const xml_node* find_bin(const xml_node& node, const std::string& name) {
  if (node.name == "coverpointBin" && node.get("name") == name)
    return &node;
  for (auto &child : node.children) {
    const xml_node* found = find_bin(*child, name);
    if (found != nullptr) return found;
  }
  return nullptr;
}

TEST(xml_64bit_values, introspection_has_the_values_in_full) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto top = static_cast<fc4sc::coverpoint_base_data_model*>(cvg.u_cvp.get_data())->bins_data[0];
    ASSERT_EQ(top->get_intervals_decimal().size(), 1u);
    EXPECT_EQ(top->get_intervals_decimal()[0].first, "18446744069414584320");
    EXPECT_EQ(top->get_intervals_decimal()[0].second, "18446744073709551615");
    auto down = static_cast<fc4sc::coverpoint_base_data_model*>(cvg.u_cvp.get_data())->bins_data[2];
    ASSERT_EQ(down->get_sequences_decimal().size(), 1u);
    EXPECT_EQ(down->get_sequences_decimal()[0], std::vector<std::string>({"18446744073709551615", "2147483648"}));
    auto min = static_cast<fc4sc::coverpoint_base_data_model*>(cvg.s_cvp.get_data())->bins_data[0];
    EXPECT_EQ(min->get_intervals_decimal()[0].first, "-9223372036854775808");
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(xml_64bit_values, written_in_full) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample_values();
    xml_printer::coverage_save("xml_64bit_values.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  auto root = fc4sc::ucis_db::read_xml_file("xml_64bit_values.xml");
  struct expected_range { const char* bin; const char* from; const char* to; uint64_t count; };
  const expected_range ranges[] = {
    {"top", "18446744069414584320", "18446744073709551615", 1},
    {"mid", "4294967296", "4294967296", 1},
    {"min", "-9223372036854775808", "-9223372036854775808", 1},
    {"neg", "-9223372036854775807", "-1", 2},
    {"high", "2147483648", "4294967295", 3},
  };
  for (auto &e : ranges) {
    const xml_node* bin = find_bin(*root, e.bin);
    ASSERT_NE(bin, nullptr) << e.bin;
    ASSERT_EQ(bin->elements("range").size(), 1u) << e.bin;
    const xml_node* range = bin->element("range");
    EXPECT_EQ(range->get("from"), e.from) << e.bin;
    EXPECT_EQ(range->get("to"), e.to) << e.bin;
    EXPECT_EQ(fc4sc::ucis_db::coverage_count(*range), e.count) << e.bin;
  }
  const xml_node* down = find_bin(*root, "down");
  ASSERT_NE(down, nullptr);
  ASSERT_EQ(down->elements("sequence").size(), 1u);
  auto values = down->element("sequence")->elements("seqValue");
  ASSERT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->text, "18446744073709551615");
  EXPECT_EQ(values[1]->text, "2147483648");
  EXPECT_EQ(fc4sc::ucis_db::coverage_count(*down->element("sequence")), 1u);
}

TEST(xml_64bit_values, loaded_back) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample_values();
    xml_printer::coverage_save("xml_64bit_values_run.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  cntxt = fc4sc::global::create_new_context();
  {
    wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // every range and sequence of the file is found in the model
    EXPECT_EQ(fc4sc::load_coverage_db("xml_64bit_values_run.xml", cntxt), 6u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cvg.s_cvp.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.s_cvp.get_bin_hit_count(1), 2u);
    EXPECT_EQ(cvg.w_cvp.get_bin_hit_count(0), 3u);
  }
  fc4sc::global::delete_context(cntxt);
}

// A file with the values in full, as other writers of UCIS files write them:
// the values are matched as integers, so a plus sign or leading zeros do not
// matter, and a value truncated to an int is not the same value
TEST(xml_64bit_values, loaded_from_another_writer) {
  {
    std::ofstream file("xml_64bit_values_other.xml");
    file <<
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<UCIS xmlns=\"UCIS\" ucisVersion=\"1.0\" writtenBy=\"t\" writtenTime=\"2026-01-01T00:00:00\">\n"
      "<sourceFiles fileName=\"a.cpp\" id=\"1\"/>\n"
      "<historyNodes historyNodeId=\"200\" logicalName=\"s\" testStatus=\"true\" date=\"2026-01-01T00:00:00\" "
      "toolCategory=\"s\" ucisVersion=\"1.0\" vendorId=\"s\" vendorTool=\"s\" vendorToolVersion=\"s\"/>\n"
      "<instanceCoverages name=\"default_scope_instance\" moduleName=\"default_scope\" key=\"1\" instanceId=\"3\">\n"
      "<id file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
      "<covergroupCoverage weight=\"1\">\n"
      "<cgInstance name=\"cvg\" key=\"2\">\n"
      "<options/>\n"
      "<cgId cgName=\"wide_values_cvg\" moduleName=\"default_scope\">\n"
      "<cginstSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
      "<cgSourceId file=\"1\" line=\"1\" inlineCount=\"1\"/>\n"
      "</cgId>\n"
      "<coverpoint name=\"u_cvp\" key=\"3\"><options/>\n"
      "<coverpointBin name=\"top\" key=\"4\" type=\"default\">\n"
      "<range from=\"+018446744069414584320\" to=\" 18446744073709551615 \"><contents coverageCount=\"7\"/></range>\n"
      "</coverpointBin>\n"
      "<coverpointBin name=\"mid\" key=\"5\" type=\"default\">\n"
      "<range from=\"0\" to=\"0\"><contents coverageCount=\"9\"/></range>\n"
      "</coverpointBin>\n"
      "<coverpointBin name=\"down\" key=\"6\" type=\"default\">\n"
      "<sequence><contents coverageCount=\"3\"/><seqValue>18446744073709551615</seqValue>"
      "<seqValue>002147483648</seqValue></sequence>\n"
      "</coverpointBin>\n"
      "</coverpoint>\n"
      "<coverpoint name=\"s_cvp\" key=\"7\"><options/>\n"
      "<coverpointBin name=\"min\" key=\"8\" type=\"default\">\n"
      "<range from=\"-9223372036854775808\" to=\"-9223372036854775808\"><contents coverageCount=\"4\"/></range>\n"
      "</coverpointBin>\n"
      "</coverpoint>\n"
      "<coverpoint name=\"w_cvp\" key=\"9\"><options/>\n"
      "<coverpointBin name=\"high\" key=\"10\" type=\"default\">\n"
      "<range from=\"-2147483648\" to=\"-1\"><contents coverageCount=\"5\"/></range>\n"
      "</coverpointBin>\n"
      "</coverpoint>\n"
      "</cgInstance>\n"
      "</covergroupCoverage>\n"
      "</instanceCoverages>\n"
      "</UCIS>\n";
  }

  auto cntxt = fc4sc::global::create_new_context();
  {
    wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // the range 0:0 of mid and the range -2147483648:-1 of high, which are
    // the values of a truncation to int, are not in the model
    EXPECT_EQ(fc4sc::load_coverage_db("xml_64bit_values_other.xml", cntxt), 3u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(0), 7u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(1), 0u);
    EXPECT_EQ(cvg.u_cvp.get_bin_hit_count(2), 3u);
    EXPECT_EQ(cvg.s_cvp.get_bin_hit_count(0), 4u);
    EXPECT_EQ(cvg.w_cvp.get_bin_hit_count(0), 0u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(xml_64bit_values, merged) {
  for (int run = 0; run < 2; ++run) {
    auto cntxt = fc4sc::global::create_new_context();
    {
      wide_values_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
      cvg.sample_values();
      xml_printer::coverage_save("xml_64bit_values_merge" + std::to_string(run) + ".xml", cntxt);
    }
    fc4sc::global::delete_context(cntxt);
  }

  // a sequence value above the largest long long is read
  fc4sc::merge_coverage_db({"xml_64bit_values_merge0.xml", "xml_64bit_values_merge1.xml"},
                           "xml_64bit_values_merged.xml");
  auto root = fc4sc::ucis_db::read_xml_file("xml_64bit_values_merged.xml");
  const xml_node* top = find_bin(*root, "top");
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(top->elements("range").size(), 1u);
  EXPECT_EQ(fc4sc::ucis_db::coverage_count(*top->element("range")), 2u);
  const xml_node* down = find_bin(*root, "down");
  ASSERT_NE(down, nullptr);
  ASSERT_EQ(down->elements("sequence").size(), 1u);
  EXPECT_EQ(fc4sc::ucis_db::coverage_count(*down->element("sequence")), 2u);
  EXPECT_EQ(down->element("sequence")->elements("seqValue")[0]->text, "18446744073709551615");
}

TEST(xml_64bit_values, parse_decimal) {
  std::string v;
  EXPECT_TRUE(fc4sc::ucis_db::parse_decimal(" +000123 ", v));
  EXPECT_EQ(v, "123");
  EXPECT_TRUE(fc4sc::ucis_db::parse_decimal("-0", v));
  EXPECT_EQ(v, "0");
  EXPECT_TRUE(fc4sc::ucis_db::parse_decimal("-00099999999999999999999", v));
  EXPECT_EQ(v, "-99999999999999999999");
  EXPECT_FALSE(fc4sc::ucis_db::parse_decimal("", v));
  EXPECT_FALSE(fc4sc::ucis_db::parse_decimal("-", v));
  EXPECT_FALSE(fc4sc::ucis_db::parse_decimal("1e3", v));
  EXPECT_FALSE(fc4sc::ucis_db::parse_decimal("0x10", v));
}
