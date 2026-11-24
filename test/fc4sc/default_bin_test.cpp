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
#include <climits>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

class default_bin_cvg : public covergroup {
public:
  CG_CONS(default_bin_cvg) {};

  int x = 0;
  int y = 0;

  void sample(int x, int y = 1) {
    this->x = x;
    this->y = y;
    covergroup::sample();
  }

  // bins low = {[0:3]}; ignore_bins ign = {7}; illegal_bins bad = {-1};
  // bins others = default;
  COVERPOINT(int, single, x) {
    bin<int>("low", interval(0, 3)),
    ignore_bin<int>("ign", 7),
    illegal_bin<int>("bad", -1),
    default_bin<int>("others")
  };

  // bins rest[] = default; bins low = {[0:3]}; bins up = (4 => 5);
  COVERPOINT(int, array, x) {
    default_bin_array<int>("rest"),
    bin<int>("low", interval(0, 3)),
    transition_bin<int>("up", trans<int>(4, 5))
  };

  COVERPOINT(int, other, y) {
    bin<int>("one", 1)
  };

  int SAMPLE_POINT(built_point, built);

  // a coverpoint built with its constructor
  coverpoint<int> built = coverpoint<int>(this, "built",
    bin<int>("two", 2),
    default_bin<int>("others")
  );

  cross<int, int> single_x_other = cross<int, int>(this, "single_x_other", &single, &other);
};

TEST(default_bin, single) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto defaults = cvg.single.get_default_bins_base();
    ASSERT_EQ(defaults.size(), 1u);
    EXPECT_EQ(defaults[0]->name(), "others");
    EXPECT_EQ(defaults[0]->bin_type(), fc4sc::bin_t::default_bin_);

    // the values in no other bin
    std::vector<fc4sc::interval_t<int>> values{
      {std::numeric_limits<int>::min(), -2}, {4, 6}, {8, std::numeric_limits<int>::max()}};
    EXPECT_EQ(defaults[0]->get_intervals_to_int(), values);

    // the default bin is not a bin of the coverpoint for the coverage
    EXPECT_EQ(cvg.single.size(), 1u);
    cvg.sample(5);
    cvg.sample(100);
    cvg.sample(-50);
    EXPECT_EQ(defaults[0]->get_hitcount(), 3u);
    EXPECT_EQ(cvg.single.get_inst_coverage(), 0);

    // values of the other bins, ignored values included, are not hit
    cvg.sample(2);
    cvg.sample(7);
    EXPECT_EQ(defaults[0]->get_hitcount(), 3u);
    EXPECT_EQ(cvg.single.get_inst_coverage(), 100);
    EXPECT_THROW(cvg.sample(-1), fc4sc::illegal_bin_sample_exception);
    EXPECT_EQ(defaults[0]->get_hitcount(), 3u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(default_bin, array) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // the default bin is declared before the bin low
    auto defaults = cvg.array.get_default_bins_base();
    ASSERT_EQ(defaults.size(), 1u);
    std::vector<fc4sc::interval_t<int>> values{
      {std::numeric_limits<int>::min(), -1}, {4, std::numeric_limits<int>::max()}};
    EXPECT_EQ(defaults[0]->get_intervals_to_int(), values);

    cvg.sample(4);
    cvg.sample(5);
    cvg.sample(5);
    cvg.sample(1);
    EXPECT_EQ(defaults[0]->get_hitcount(), 3u);
    auto cvp_data = static_cast<fc4sc::coverpoint_base_data_model*>(cvg.array.get_data());
    auto data = static_cast<fc4sc::default_bin_data_model<int>*>(cvp_data->default_bins_data[0]);
    EXPECT_EQ(data->value_hits, (std::map<int, uint64_t>{{4, 1}, {5, 2}}));

    // the value 5 hits the transition bin up and the default bin
    EXPECT_EQ(cvg.array.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cvg.array.size(), 2u);
    EXPECT_EQ(cvg.array.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(default_bin, constructor) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cvg.built.size(), 1u);
    cvg.built_point = 3;
    cvg.sample(0);
    auto defaults = cvg.built.get_default_bins_base();
    ASSERT_EQ(defaults.size(), 1u);
    EXPECT_EQ(defaults[0]->get_hitcount(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(default_bin, not_crossed) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // 5 is in the default bin only: the cross is not hit
    cvg.sample(5, 1);
    EXPECT_TRUE(cvg.single_x_other.get_cross_bins().empty());
    EXPECT_EQ(cvg.single_x_other.get_misses(), 1u);
    cvg.sample(2, 1);
    EXPECT_EQ(cvg.single_x_other.get_cross_bins().size(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(default_bin, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(5);
    cvg.sample(9);
    cvg.sample(9);
    xml_printer::coverage_save("default_bin_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("default_bin_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  // a default bin is not counted in the coverage: it is written as an
  // ignore bin, with the values it holds
  size_t pos = xml.find("<coverpointBin name=\"others\"");
  ASSERT_NE(pos, std::string::npos);
  size_t end = xml.find("</coverpointBin>", pos);
  const std::string others = xml.substr(pos, end - pos);
  EXPECT_NE(others.find("type=\"ignore\""), std::string::npos);
  // 5 in [4:6], 9 twice in [8:max]
  EXPECT_NE(others.find("from=\"4\""), std::string::npos);
  EXPECT_NE(others.find("coverageCount=\"1\""), std::string::npos);
  EXPECT_NE(others.find("coverageCount=\"2\""), std::string::npos);

  // a default bin array has a bin for each value hit
  pos = xml.find("<coverpointBin name=\"rest[9]\"");
  ASSERT_NE(pos, std::string::npos);
  end = xml.find("</coverpointBin>", pos);
  const std::string rest9 = xml.substr(pos, end - pos);
  EXPECT_NE(rest9.find("type=\"ignore\""), std::string::npos);
  EXPECT_NE(rest9.find("coverageCount=\"2\""), std::string::npos);
  EXPECT_NE(xml.find("<coverpointBin name=\"rest[5]\""), std::string::npos);
  EXPECT_EQ(xml.find("<coverpointBin name=\"rest[4]\""), std::string::npos);
}

class default_bin_wide_cvg : public covergroup {
public:
  CG_CONS(default_bin_wide_cvg) {};

  unsigned int u = 0;
  uint64_t w = 0;

  COVERPOINT(unsigned int, cvp_u, u) {
    bin<unsigned int>("low", interval(0u, 7u)),
    default_bin<unsigned int>("others")
  };

  COVERPOINT(uint64_t, cvp_w, w) {
    bin<uint64_t>("low", interval<uint64_t>(0, 7)),
    default_bin_array<uint64_t>("rest")
  };
};

// The values of the default bins of unsigned and 64-bit coverpoints do not
// fit in an int, and are written as they are
TEST(default_bin, wide_types_written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_wide_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.u = 4000000000u;
    // two values with the same low 32 bits
    cvg.w = 0x100000008ull;
    cvg.sample();
    cvg.w = 0x200000008ull;
    cvg.sample();
    xml_printer::coverage_save("default_bin_wide_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("default_bin_wide_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  size_t pos = xml.find("<coverpointBin name=\"others\"");
  ASSERT_NE(pos, std::string::npos);
  const std::string others = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(others.find("from=\"8\" \nto =\"4294967295\""), std::string::npos);
  EXPECT_NE(others.find("coverageCount=\"2\""), std::string::npos);

  EXPECT_NE(xml.find("<coverpointBin name=\"rest[4294967304]\""), std::string::npos);
  EXPECT_NE(xml.find("<coverpointBin name=\"rest[8589934600]\""), std::string::npos);
}

class default_bin_array_only_cvg : public covergroup {
public:
  CG_CONS(default_bin_array_only_cvg) {};

  int x = 0;

  COVERPOINT(int, only_array, x) {
    default_bin_array<int>("rest")
  };

  COVERPOINT(int, after_bins, x) {
    bin<int>("low", interval(INT_MIN, 7)),
    default_bin_array<int>("high")
  };
};

// A coverpoint needs at least one bin in the UCIS XML file: a default bin
// array without hits has a bin for its first value, with no hits
TEST(default_bin, array_without_hits_written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    default_bin_array_only_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    xml_printer::coverage_save("default_bin_array_only_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("default_bin_array_only_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  size_t cvp = xml.find("<coverpoint name=\"only_array\"");
  ASSERT_NE(cvp, std::string::npos);
  const std::string only_array = xml.substr(cvp, xml.find("</coverpoint>", cvp) - cvp);
  size_t pos = only_array.find("<coverpointBin name=\"rest[-2147483648]\"");
  ASSERT_NE(pos, std::string::npos);
  const std::string rest = only_array.substr(pos, only_array.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(rest.find("type=\"ignore\""), std::string::npos);
  EXPECT_NE(rest.find("from=\"-2147483648\" \nto =\"-2147483648\""), std::string::npos);
  EXPECT_NE(rest.find("coverageCount=\"0\""), std::string::npos);

  cvp = xml.find("<coverpoint name=\"after_bins\"");
  ASSERT_NE(cvp, std::string::npos);
  const std::string after_bins = xml.substr(cvp, xml.find("</coverpoint>", cvp) - cvp);
  EXPECT_NE(after_bins.find("<coverpointBin name=\"high[8]\""), std::string::npos);
}
