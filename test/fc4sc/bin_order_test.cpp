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

class bin_order_test : public covergroup {
public:
  CG_CONS(bin_order_test) {};

  int SAMPLE_POINT(sample_point_1, cvp_args);
  int sample_point_2 = 0;

  // the same bins, given as constructor arguments and with COVERPOINT
  coverpoint<int> cvp_args = coverpoint<int> (this, "cvp_args",
    bin<int>("one", 1),
    illegal_bin<int>("illegal_seven", 7),
    bin_array<int>("many", 2, interval(2, 3)),
    ignore_bin<int>("ignore_eight", 8),
    bin<int>("four", 4),
    illegal_bin<int>("illegal_nine", 9),
    ignore_bin<int>("ignore_ten", 10)
  );
  COVERPOINT(int, cvp_list, sample_point_2) {
    bin<int>("one", 1),
    illegal_bin<int>("illegal_seven", 7),
    bin_array<int>("many", 2, interval(2, 3)),
    ignore_bin<int>("ignore_eight", 8),
    bin<int>("four", 4),
    illegal_bin<int>("illegal_nine", 9),
    ignore_bin<int>("ignore_ten", 10)
  };
};

namespace {

// Reads the names of the bins of a coverpoint, in the order they are written
std::vector<std::string> read_bin_names(const std::string& xml, const std::string& cvp) {
  size_t start = xml.find("<coverpoint name=\"" + cvp + "\"");
  const std::string element = xml.substr(start, xml.find("</coverpoint>", start) - start);

  std::vector<std::string> names;
  const std::string open = "<coverpointBin name=\"";
  size_t pos = 0;
  while ((pos = element.find(open, pos)) != std::string::npos) {
    pos += open.size();
    names.push_back(element.substr(pos, element.find('"', pos) - pos));
  }
  return names;
}

} // namespace

// The bins of a coverpoint are kept in the order in which they are given,
// whether they are given as constructor arguments or with COVERPOINT. The
// bin indices, and so the cross bins, refer to the bins in that order.
TEST(bin_order, declaration_order) {
  auto cntxt = fc4sc::global::create_new_context();
  bin_order_test cg("cg", __FILE__, __LINE__, cntxt);

  for (int value : {1, 3, 3, 4, 4, 4}) {
    cg.sample_point_1 = value;
    cg.sample_point_2 = value;
    cg.sample();
  }

  for (auto* cvp : {&cg.cvp_args, &cg.cvp_list}) {
    ASSERT_EQ(cvp->size(), 4u) << cvp->name();
    EXPECT_EQ(cvp->get_bin_hit_count(0), 1u) << cvp->name(); // one
    EXPECT_EQ(cvp->get_bin_hit_count(1), 0u) << cvp->name(); // many[0]
    EXPECT_EQ(cvp->get_bin_hit_count(2), 2u) << cvp->name(); // many[1]
    EXPECT_EQ(cvp->get_bin_hit_count(3), 3u) << cvp->name(); // four
  }

  xml_printer::coverage_save("bin_order.xml", cntxt);
  std::ifstream file("bin_order.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  // the regular bins first, then the illegal and the ignore bins
  const std::vector<std::string> expected = {
    "one", "many[0]", "many[1]", "four",
    "illegal_seven", "illegal_nine", "ignore_eight", "ignore_ten"};
  EXPECT_EQ(read_bin_names(xml, "cvp_args"), expected);
  EXPECT_EQ(read_bin_names(xml, "cvp_list"), expected);

  fc4sc::global::delete_context(cntxt);
}
