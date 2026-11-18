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

class cross_order_test : public covergroup {
public:
  CG_CONS(cross_order_test) {};

  int a = 0;
  int b = 0;
  int c = 0;

  COVERPOINT(int, cvp_a, a) {
    bin<int>("a0", 0),
    bin<int>("a1", 1)
  };
  COVERPOINT(int, cvp_b, b) {
    bin<int>("b0", 0),
    bin<int>("b1", 1),
    bin<int>("b2", 2)
  };
  COVERPOINT(int, cvp_c, c) {
    bin<int>("c0", 0),
    bin<int>("c1", 1),
    bin<int>("c2", 2),
    bin<int>("c3", 3)
  };

  cross<int,int,int> cross_abc = cross<int,int,int> (this, "cross_abc",
    &cvp_a, &cvp_b, &cvp_c);
};

// The coverpoints of a cross are kept in the order in which they are given
// to it, as in a SystemVerilog cross, and so are the bin indices of each
// cross bin.
TEST(cross_order, declaration_order) {
  auto cntxt = fc4sc::global::create_new_context();
  cross_order_test cg("cg", __FILE__, __LINE__, cntxt);

  cg.a = 1;
  cg.b = 2;
  cg.c = 3;
  cg.sample();

  const auto& cvps = cg.cross_abc.get_cross_coverpoints();
  ASSERT_EQ(cvps.size(), 3u);
  EXPECT_EQ(cvps[0]->name(), "cvp_a");
  EXPECT_EQ(cvps[1]->name(), "cvp_b");
  EXPECT_EQ(cvps[2]->name(), "cvp_c");

  const auto& bins = cg.cross_abc.get_cross_bins();
  ASSERT_EQ(bins.size(), 1u);
  EXPECT_EQ(bins.begin()->first, (std::vector<size_t>{1, 2, 3}));
  EXPECT_EQ(bins.begin()->second, 1u);

  // The UCIS file lists the crossed coverpoints in the same order, which is
  // the order of the index elements of each cross bin.
  xml_printer::coverage_save("cross_order.xml", cntxt);
  std::ifstream file("cross_order.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  std::vector<std::string> exprs;
  size_t pos = 0;
  const std::string open = "<crossExpr>";
  const std::string close = "</crossExpr>";
  while ((pos = xml.find(open, pos)) != std::string::npos) {
    pos += open.size();
    exprs.push_back(xml.substr(pos, xml.find(close, pos) - pos));
  }
  EXPECT_EQ(exprs, (std::vector<std::string>{"cvp_a", "cvp_b", "cvp_c"}));

  fc4sc::global::delete_context(cntxt);
}
