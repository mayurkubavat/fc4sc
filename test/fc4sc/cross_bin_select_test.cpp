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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

// SystemVerilog:
//   cvp_a: coverpoint a { bins zero = {0}; bins low[] = {[1:3]}; bins high = {[4:7]}; }
//   cvp_b: coverpoint b { bins x = {0}; bins y = {1}; }
//   cross cvp_a, cvp_b {
//     bins zero_any = binsof(cvp_a.zero);
//     bins low_y = binsof(cvp_a.low) && binsof(cvp_b.y);
//     bins hi = binsof(cvp_a) intersect {[6:9]} ||
//               binsof(cvp_a.zero) && binsof(cvp_b.x);
//     ignore_bins ign = binsof(cvp_a.high) && binsof(cvp_b.x);
//     illegal_bins bad = binsof(cvp_a.low[2]) && ! binsof(cvp_b) intersect {1};
//   }
class cross_select_cvg : public covergroup {
public:
  CG_CONS(cross_select_cvg) {};

  int a = 0;
  int b = 0;

  void sample(int a, int b) {
    this->a = a;
    this->b = b;
    covergroup::sample();
  }

  COVERPOINT(int, cvp_a, a) {
    bin<int>("zero", 0),
    bin_array<int>("low", 3, interval(1, 3)),
    bin<int>("high", interval(4, 7))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("x", 0),
    bin<int>("y", 1)
  };

  cross<int, int> a_x_b = cross<int, int>(this, "a_x_b", &cvp_a, &cvp_b,
    cross_bin("zero_any", binsof<int>(&cvp_a, "zero")),
    cross_bin("low_y", binsof<int>(&cvp_a, "low") && binsof<int>(&cvp_b, "y")),
    cross_bin("hi", binsof<int>(&cvp_a).intersect(interval(6, 9)) ||
                    (binsof<int>(&cvp_a, "zero") && binsof<int>(&cvp_b, "x"))),
    ignore_cross_bin("ign", binsof<int>(&cvp_a, "high") && binsof<int>(&cvp_b, "x")),
    illegal_cross_bin("bad", binsof<int>(&cvp_a, "low[2]") && !binsof<int>(&cvp_b).intersect(1))
  );

  // without user defined bins, and without a name
  cross<int, int> plain = cross<int, int>(this, &cvp_a, &cvp_b);
};

static const fc4sc::cross_bin_data_model& user_bin(fc4sc::cross_base& crs, const std::string& name)
{
  auto data = static_cast<fc4sc::cross_base_data_model*>(crs.get_data());
  for (auto& bin : data->user_bins)
    if (bin.name == name) return bin;
  throw std::invalid_argument("no cross bin " + name);
}

typedef std::map<std::vector<size_t>, uint64_t> cross_bins_t;

TEST(cross_bin_select, bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_select_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto& x = cvg.a_x_b;

    // 10 products: 3 in the user defined bins, 1 ignored, 1 illegal and
    // 2 automatically created bins for (low[0], x) and (low[1], x)
    EXPECT_EQ(user_bin(x, "zero_any").size, 2u);
    EXPECT_EQ(user_bin(x, "low_y").size, 3u);
    // (zero, x) and (high, y): (high, x) is ignored
    EXPECT_EQ(user_bin(x, "hi").size, 2u);
    EXPECT_EQ(user_bin(x, "ign").size, 1u);
    EXPECT_EQ(user_bin(x, "bad").size, 1u);

    int covered = -1, total = -1;
    EXPECT_EQ(x.get_inst_coverage(covered, total), 0);
    EXPECT_EQ(total, 5);
    EXPECT_EQ(covered, 0);

    // (zero, x) is in zero_any and in hi
    cvg.sample(0, 0);
    EXPECT_EQ(user_bin(x, "zero_any").hits, 1u);
    EXPECT_EQ(user_bin(x, "hi").hits, 1u);
    EXPECT_TRUE(x.get_cross_bins().empty());
    EXPECT_EQ(x.get_inst_coverage(), 40);

    cvg.sample(0, 1);
    EXPECT_EQ(user_bin(x, "zero_any").hits, 2u);
    EXPECT_EQ(x.get_inst_coverage(), 40);

    // an automatically created bin
    cvg.sample(1, 0);
    EXPECT_EQ(x.get_cross_bins(), (cross_bins_t{{{1, 0}, 1}}));
    EXPECT_EQ(x.get_inst_coverage(), 60);

    // an ignored product is in no bin
    cvg.sample(6, 0);
    EXPECT_EQ(user_bin(x, "ign").hits, 1u);
    EXPECT_EQ(user_bin(x, "hi").hits, 1u);
    EXPECT_EQ(x.get_cross_bins().size(), 1u);
    EXPECT_EQ(x.get_misses(), 1u);
    EXPECT_EQ(x.get_inst_coverage(), 60);

    cvg.sample(2, 1);
    EXPECT_EQ(user_bin(x, "low_y").hits, 1u);
    EXPECT_EQ(x.get_inst_coverage(), 80);

    cvg.sample(2, 0);
    EXPECT_EQ(x.get_inst_coverage(covered, total), 100);
    EXPECT_EQ(covered, 5);
    EXPECT_EQ(total, 5);

    // the cross without user defined bins has a bin for each product
    EXPECT_EQ(cvg.plain.get_inst_coverage(covered, total), 60);
    EXPECT_EQ(total, 10);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_bin_select, illegal_bin) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_select_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
#ifdef FC4SC_NO_THROW
    EXPECT_NO_THROW(cvg.sample(3, 0));
#else
    EXPECT_THROW(cvg.sample(3, 0), fc4sc::illegal_bin_sample_exception);
#endif
    EXPECT_EQ(user_bin(cvg.a_x_b, "bad").hits, 1u);
    EXPECT_TRUE(cvg.a_x_b.get_cross_bins().empty());
    // (low[2], y) is in low_y
    cvg.sample(3, 1);
    EXPECT_EQ(user_bin(cvg.a_x_b, "low_y").hits, 1u);
    EXPECT_EQ(user_bin(cvg.a_x_b, "bad").hits, 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

// A sample hits the bins lo and hi of cvp_c: the user defined bin is hit
// once, the automatically created bins once each.
class cross_select_overlap_cvg : public covergroup {
public:
  CG_CONS(cross_select_overlap_cvg) {};

  int SAMPLE_POINT(c, cvp_c);
  int SAMPLE_POINT(d, cvp_d);

  void sample(int c, int d) {
    this->c = c;
    this->d = d;
    covergroup::sample();
  }

  coverpoint<int> cvp_c = coverpoint<int>(this, "cvp_c",
    bin<int>("lo", interval(0, 5)),
    bin<int>("hi", interval(3, 8)),
    bin<int>("top", 9)
  );
  coverpoint<int> cvp_d = coverpoint<int>(this, "cvp_d",
    bin<int>("z", 0)
  );

  cross<int, int> c_x_d = cross<int, int>(this, "c_x_d", &cvp_c, &cvp_d,
    cross_bin("lo_hi", binsof<int>(cvp_c).intersect(std::vector<int>{1, 2}, interval(7, 8)))
  );
};

TEST(cross_bin_select, overlapping_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_select_overlap_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(user_bin(cvg.c_x_d, "lo_hi").size, 2u);
    cvg.sample(4, 0);
    EXPECT_EQ(user_bin(cvg.c_x_d, "lo_hi").hits, 1u);
    cvg.sample(9, 0);
    EXPECT_EQ(cvg.c_x_d.get_cross_bins(), (cross_bins_t{{{2, 0}, 1}}));
    int covered = 0, total = 0;
    EXPECT_EQ(cvg.c_x_d.get_inst_coverage(covered, total), 100);
    EXPECT_EQ(total, 2);
  }
  fc4sc::global::delete_context(cntxt);
}

class cross_select_bad_bin : public covergroup {
public:
  CG_CONS(cross_select_bad_bin) {};
  int a = 0;
  COVERPOINT(int, cvp_a, a) { bin<int>("one", 1) };
  COVERPOINT(int, cvp_b, a) { bin<int>("one", 1) };
  cross<int> only_a = cross<int>(this, "only_a", &cvp_a,
    cross_bin("two", binsof<int>(&cvp_a, "two")));
};

class cross_select_other_cvp : public covergroup {
public:
  CG_CONS(cross_select_other_cvp) {};
  int a = 0;
  COVERPOINT(int, cvp_a, a) { bin<int>("one", 1) };
  COVERPOINT(int, cvp_b, a) { bin<int>("one", 1) };
  cross<int> only_a = cross<int>(this, "only_a", &cvp_a,
    cross_bin("b", binsof<int>(&cvp_b)));
};

TEST(cross_bin_select, bad_select) {
  auto cntxt = fc4sc::global::create_new_context();
  // a bin which is not in the coverpoint, and a coverpoint not in the cross
  EXPECT_THROW(cross_select_bad_bin("bad_bin", __FILE__, __LINE__, cntxt), std::invalid_argument);
  EXPECT_THROW(cross_select_other_cvp("other_cvp", __FILE__, __LINE__, cntxt), std::invalid_argument);
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_bin_select, merge_instances) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_select_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    cross_select_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;
    inst1.sample(0, 0);  // zero_any, hi
    inst1.sample(1, 0);  // <low[0], x>
    inst2.sample(2, 1);  // low_y
    inst2.sample(1, 0);  // <low[0], x>
    int covered = 0, total = 0;
    EXPECT_EQ(inst1.a_x_b.get_coverage(covered, total), 80);
    EXPECT_EQ(covered, 4);
    EXPECT_EQ(total, 5);

    inst1.type_option().merge_instances = false;
    // the average of 60 and 40
    EXPECT_EQ(inst1.a_x_b.get_coverage(), 50);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_bin_select, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_select_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(0, 0);
    cvg.sample(1, 0);
    cvg.sample(6, 0);
    cvg.a_x_b.option().cross_num_print_missing = 100;
    xml_printer::coverage_save("cross_bin_select_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("cross_bin_select_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  size_t start = xml.find("<cross name=\"a_x_b\"");
  ASSERT_NE(start, std::string::npos);
  const std::string crs = xml.substr(start, xml.find("</cross>", start) - start);

  auto cross_bin = [&](const std::string& name) {
    size_t pos = crs.find("<crossBin \nname=\"" + name + "\"");
    if (pos == std::string::npos) return std::string();
    return crs.substr(pos, crs.find("</crossBin>", pos) - pos);
  };

  // the automatically created bins only
  EXPECT_NE(cross_bin("&lt;1,0&gt;").find("coverageCount=\"1\""), std::string::npos);
  EXPECT_NE(cross_bin("&lt;2,0&gt;").find("coverageCount=\"0\""), std::string::npos);
  EXPECT_EQ(cross_bin("&lt;0,0&gt;"), "");
  EXPECT_EQ(cross_bin("&lt;4,0&gt;"), "");
  EXPECT_EQ(cross_bin("&lt;3,0&gt;"), "");

  // the user defined bins, with their types
  const std::string zero_any = cross_bin("zero_any");
  EXPECT_NE(zero_any.find("coverageCount=\"1\""), std::string::npos);
  EXPECT_EQ(zero_any.find("type="), std::string::npos);
  EXPECT_NE(cross_bin("ign").find("type=\"ignore\""), std::string::npos);
  EXPECT_NE(cross_bin("ign").find("coverageCount=\"1\""), std::string::npos);
  EXPECT_NE(cross_bin("bad").find("type=\"illegal\""), std::string::npos);
  EXPECT_NE(cross_bin("low_y").find("coverageCount=\"0\""), std::string::npos);
}
