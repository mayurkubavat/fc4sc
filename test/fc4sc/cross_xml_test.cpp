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
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

class cross_xml_test : public covergroup {
public:
  CG_CONS(cross_xml_test) {};

  int SAMPLE_POINT(sample_point_1, cvp1);
  int SAMPLE_POINT(sample_point_2, cvp2);

  void sample(const int& x, const int& y) {
    this->sample_point_1 = x;
    this->sample_point_2 = y;
    covergroup::sample();
  }

  coverpoint<int> cvp1 = coverpoint<int> (this, "cvp1",
    bin<int>("one", 1),
    bin<int>("two", 2)
  );
  coverpoint<int> cvp2 = coverpoint<int> (this, "cvp2",
    bin<int>("one", 1),
    bin<int>("two", 2),
    bin<int>("three", 3)
  );
  cross<int,int> cross1 = cross<int,int> (this, "cross1", &cvp1, &cvp2);
};

// cvp2 has no regular bins, so the cross of cvp1 and cvp2 has no bins
class cross_xml_no_bins_test : public covergroup {
public:
  CG_CONS(cross_xml_no_bins_test) {};

  int SAMPLE_POINT(sample_point_1, cvp1);
  int SAMPLE_POINT(sample_point_2, cvp2);

  coverpoint<int> cvp1 = coverpoint<int> (this, "cvp1",
    bin<int>("one", 1),
    bin<int>("two", 2)
  );
  coverpoint<int> cvp2 = coverpoint<int> (this, "cvp2",
    bin<int>("empty", 0).with([](int) { return false; })
  );
  cross<int,int> cross1 = cross<int,int> (this, "cross1", &cvp1, &cvp2);
};

namespace {

struct cross_bin {
  std::vector<int> indices;
  int count;
};

// Returns the text between 'open' and 'close', searching from 'pos' on.
// 'pos' is moved behind 'close', or set to npos if there is no match.
std::string next_element(const std::string& text, const std::string& open,
                         const std::string& close, size_t& pos) {
  size_t start = text.find(open, pos);
  size_t stop = (start == std::string::npos) ? start
                                             : text.find(close, start + open.size());
  if (stop == std::string::npos) {
    pos = std::string::npos;
    return "";
  }
  pos = stop + close.size();
  start += open.size();
  return text.substr(start, stop - start);
}

std::string read_file(const std::string& file_name) {
  std::ifstream file(file_name);
  std::stringstream content;
  content << file.rdbuf();
  return content.str();
}

// Reads the cross bins which coverage_save wrote to the given file
std::vector<cross_bin> read_cross_bins(const std::string& xml) {
  std::vector<cross_bin> bins;
  size_t pos = 0;
  while (true) {
    const std::string element = next_element(xml, "<crossBin", "</crossBin>", pos);
    if (pos == std::string::npos) break;

    cross_bin bin;
    size_t index_pos = 0;
    while (true) {
      const std::string index = next_element(element, "<index>", "</index>", index_pos);
      if (index_pos == std::string::npos) break;
      bin.indices.push_back(std::stoi(index));
    }
    size_t count_pos = 0;
    bin.count = std::stoi(next_element(element, "coverageCount=\"", "\"", count_pos));
    bins.push_back(bin);
  }
  return bins;
}

// Reads the names of the crossed coverpoints, in the order of the indices
std::vector<std::string> read_cross_exprs(const std::string& xml) {
  std::vector<std::string> names;
  size_t pos = 0;
  while (true) {
    const std::string name = next_element(xml, "<crossExpr>", "</crossExpr>", pos);
    if (pos == std::string::npos) break;
    names.push_back(name);
  }
  return names;
}

// Reads the names of the bins of a coverpoint, in the order they are written
std::vector<std::string> read_bin_names(const std::string& xml, const std::string& cvp) {
  size_t pos = 0;
  const std::string element =
      next_element(xml, "<coverpoint name=\"" + cvp + "\"", "</coverpoint>", pos);

  std::vector<std::string> names;
  pos = 0;
  while (true) {
    const std::string name = next_element(element, "<coverpointBin name=\"", "\"", pos);
    if (pos == std::string::npos) break;
    names.push_back(name);
  }
  return names;
}


// Writes the coverage database and returns the hit count of every cross bin
// in it, keyed by the names of its coverpoint bins: "cvp1=<bin> cvp2=<bin>".
std::map<std::string, int> save_cross_bins(const std::string& file_name,
                                           fc4sc::global* cntxt) {
  xml_printer::coverage_save(file_name, cntxt);
  const std::string xml = read_file(file_name);
  auto bins = read_cross_bins(xml);
  auto exprs = read_cross_exprs(xml);

  std::map<std::string, int> hits;
  EXPECT_EQ(exprs.size(), 2u);
  for (auto& bin : bins) {
    EXPECT_EQ(bin.indices.size(), exprs.size());
    std::map<std::string, std::string> names;
    for (size_t i = 0; i < exprs.size() && i < bin.indices.size(); i++) {
      auto bin_names = read_bin_names(xml, exprs[i]);
      EXPECT_LT(static_cast<size_t>(bin.indices[i]), bin_names.size());
      if (static_cast<size_t>(bin.indices[i]) < bin_names.size())
        names[exprs[i]] = bin_names[bin.indices[i]];
    }
    const std::string key = "cvp1=" + names["cvp1"] + " cvp2=" + names["cvp2"];
    EXPECT_EQ(hits.count(key), 0u) << key << " is written twice";
    hits[key] = bin.count;
  }
  return hits;
}

// Number of cross bins written: the bins hit, and the missing bins up to
// option().cross_num_print_missing, or all of them with
// FC4SC_SAVE_ALL_CROSS_BINS. There is always at least one.
size_t expected_bins(size_t hit, size_t missing, unsigned int print_missing) {
#ifdef FC4SC_SAVE_ALL_CROSS_BINS
  (void)print_missing;
  return hit + missing;
#else
  size_t written = hit + std::min<size_t>(missing, print_missing);
  return (written == 0) ? 1 : written;
#endif
}

void sample_some(cross_xml_test& cg) {
  cg.sample(1, 3);
  cg.sample(1, 3);
  cg.sample(2, 1);
  cg.sample(7, 1); // not in a bin of cvp1, no cross bin is hit
}

} // namespace

// A cross which was never hit is written with at least one bin, each with
// the index of a bin of every crossed coverpoint, as the schema requires.
TEST(cross_xml, no_hits) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_xml_test cg("cg", __FILE__, __LINE__, cntxt);

  auto hits = save_cross_bins("cross_xml_no_hits.xml", cntxt);

  EXPECT_EQ(hits.size(), expected_bins(0, 6, 0));
  for (auto& hit : hits)
    EXPECT_EQ(hit.second, 0) << hit.first;

  fc4sc::global::delete_context(cntxt);
}

// The bins which were hit are always written, with their hit counts.
TEST(cross_xml, some_hits) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_xml_test cg("cg", __FILE__, __LINE__, cntxt);
  sample_some(cg);

  auto hits = save_cross_bins("cross_xml_some_hits.xml", cntxt);

  EXPECT_EQ(hits.size(), expected_bins(2, 4, 0));
  EXPECT_EQ(hits["cvp1=one cvp2=three"], 2);
  EXPECT_EQ(hits["cvp1=two cvp2=one"], 1);

  fc4sc::global::delete_context(cntxt);
}

// option().cross_num_print_missing sets how many missing bins are written in
// addition to the bins which were hit.
TEST(cross_xml, print_missing) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_xml_test cg("cg", __FILE__, __LINE__, cntxt);
  cg.cross1.option().cross_num_print_missing = 3;
  sample_some(cg);

  auto hits = save_cross_bins("cross_xml_print_missing.xml", cntxt);

  EXPECT_EQ(hits.size(), expected_bins(2, 4, 3));
  int missing = 0;
  for (auto& hit : hits)
    if (hit.second == 0) missing++;
  EXPECT_EQ(static_cast<size_t>(missing), hits.size() - 2);

  // all bins written: the coverage computed from the file is the library's
  cg.cross1.option().cross_num_print_missing = 6;
  hits = save_cross_bins("cross_xml_print_missing.xml", cntxt);
  ASSERT_EQ(hits.size(), 6u);
  EXPECT_DOUBLE_EQ(cg.cross1.get_inst_coverage(), 100.0 * 2 / hits.size());

  fc4sc::global::delete_context(cntxt);
}

// Every cross bin has a name of its own, which is made of its indices.
// Tools which read UCIS files identify the bins of a cross by their names.
TEST(cross_xml, bin_names) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_xml_test cg("cg", __FILE__, __LINE__, cntxt);
  cg.cross1.option().cross_num_print_missing = 6;
  sample_some(cg);

  xml_printer::coverage_save("cross_xml_bin_names.xml", cntxt);
  const std::string xml = read_file("cross_xml_bin_names.xml");

  std::set<std::string> names;
  size_t pos = 0;
  while (true) {
    const std::string element = next_element(xml, "<crossBin", "</crossBin>", pos);
    if (pos == std::string::npos) break;

    size_t name_pos = 0;
    const std::string name = next_element(element, "name=\"", "\"", name_pos);
    std::string expected = "&lt;";
    size_t index_pos = 0;
    while (true) {
      const std::string index = next_element(element, "<index>", "</index>", index_pos);
      if (index_pos == std::string::npos) break;
      expected += (expected == "&lt;" ? "" : ",") + index;
    }
    expected += "&gt;";

    EXPECT_EQ(name, expected);
    names.insert(name);
  }
  EXPECT_EQ(names.size(), 6u);

  fc4sc::global::delete_context(cntxt);
}

// A cross of a coverpoint without regular bins has no bins. The schema
// requires at least one crossBin, so a single ignore bin is written which
// has the index -1, for no bin, for every crossed coverpoint.
TEST(cross_xml, no_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_xml_no_bins_test cg("cg", __FILE__, __LINE__, cntxt);
  cg.sample_point_1 = 1;
  cg.sample_point_2 = 5;
  cg.sample();

  xml_printer::coverage_save("cross_xml_no_bins.xml", cntxt);
  const std::string xml = read_file("cross_xml_no_bins.xml");
  auto bins = read_cross_bins(xml);

  ASSERT_EQ(bins.size(), 1u);
  EXPECT_EQ(bins[0].indices, std::vector<int>(read_cross_exprs(xml).size(), -1));
  EXPECT_EQ(bins[0].count, 0);

  size_t pos = 0;
  const std::string element = next_element(xml, "<crossBin", "</crossBin>", pos);
  size_t type_pos = 0;
  EXPECT_EQ(next_element(element, "type=\"", "\"", type_pos), "ignore");

  fc4sc::global::delete_context(cntxt);
}
