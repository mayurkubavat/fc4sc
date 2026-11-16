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

#include "fc4sc.hpp"
#include "gtest/gtest.h"

class merge_instances_cvg : public covergroup {
public:
  CG_CONS(merge_instances_cvg) {};

  int SAMPLE_POINT(x, cvp_x);
  int SAMPLE_POINT(y, cvp_y);

  void sample(int x, int y) {
    this->x = x;
    this->y = y;
    covergroup::sample();
  }

  coverpoint<int> cvp_x = coverpoint<int> (this,
    bin<int>("a", 1),
    bin<int>("b", 2),
    bin<int>("c", 3),
    bin<int>("d", 4)
  );
  coverpoint<int> cvp_y = coverpoint<int> (this,
    bin<int>("one", 1),
    bin<int>("two", 2)
  );
  cross<int,int> x_y = cross<int,int> (this, "x_y", &cvp_x, &cvp_y);
};

class merge_instances_cvp_only : public covergroup {
public:
  CG_CONS(merge_instances_cvp_only) {};

  int x = 0;
  COVERPOINT(int, cvp_x, x) {
    bin<int>("a", 1),
    bin<int>("b", 2),
    bin<int>("c", 3),
    bin<int>("d", 4)
  };
};

// Instance 1 covers bins a and b, instance 2 covers bins b and c.
static void sample_two_instances(merge_instances_cvp_only& inst1,
                                 merge_instances_cvp_only& inst2) {
  inst1.x = 1; inst1.sample();
  inst1.x = 2; inst1.sample();
  inst2.x = 2; inst2.sample();
  inst2.x = 3; inst2.sample();
}

TEST(merge_instances, weighted_average_by_default) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvp_only inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvp_only inst2("inst2", __FILE__, __LINE__, cntxt);
    sample_two_instances(inst1, inst2);

    EXPECT_FALSE(inst1.type_option().merge_instances);
    EXPECT_EQ(inst1.get_inst_coverage(), 50);
    EXPECT_EQ(inst2.get_inst_coverage(), 50);
    EXPECT_EQ(inst1.get_coverage(), 50);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(merge_instances, coverpoint_bins_are_merged) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvp_only inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvp_only inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;
    sample_two_instances(inst1, inst2);

    // instance coverage does not change
    EXPECT_EQ(inst1.get_inst_coverage(), 50);
    EXPECT_EQ(inst2.get_inst_coverage(), 50);

    // bins a, b and c of the 4 merged bins are covered
    int hit = 0, total = 0;
    EXPECT_EQ(inst1.get_coverage(), 75);
    EXPECT_EQ(inst2.get_coverage(hit, total), 75);
    EXPECT_EQ(hit, 3);
    EXPECT_EQ(total, 4);

    inst2.x = 4; inst2.sample();
    EXPECT_EQ(inst1.get_coverage(), 100);
    EXPECT_EQ(inst2.get_inst_coverage(), 75);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(merge_instances, hits_are_added_for_at_least) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvp_only inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvp_only inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;
    inst1.cvp_x.option().at_least = 2;
    inst2.cvp_x.option().at_least = 2;

    // one hit of bin a in each instance: neither instance covers it
    inst1.x = 1; inst1.sample();
    inst2.x = 1; inst2.sample();

    EXPECT_EQ(inst1.get_inst_coverage(), 0);
    EXPECT_EQ(inst2.get_inst_coverage(), 0);
    EXPECT_EQ(inst1.get_coverage(), 25);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(merge_instances, cross_bins_are_merged) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvg inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvg inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;

    inst1.sample(1, 1);
    inst1.sample(2, 2);
    inst2.sample(2, 2);
    inst2.sample(3, 1);

    // cvp_x: 3 of 4, cvp_y: 2 of 2, x_y: 3 of 8 merged bins
    int hit = 0, total = 0;
    EXPECT_DOUBLE_EQ(inst1.get_coverage(hit, total), (75.0 + 100.0 + 37.5) / 3);
    EXPECT_EQ(hit, 3 + 2 + 3);
    EXPECT_EQ(total, 4 + 2 + 8);

    // the same hits without merging: each instance covers 2 of 4, 2 of 2
    // and 2 of 8 bins
    inst1.type_option().merge_instances = false;
    EXPECT_DOUBLE_EQ(inst1.get_coverage(hit, total), (50.0 + 100.0 + 25.0) / 3);
    EXPECT_EQ(hit, 2 * (2 + 2 + 2));
    EXPECT_EQ(total, 2 * (4 + 2 + 8));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(merge_instances, goal_of_the_type_applies) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvp_only inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvp_only inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;
    sample_two_instances(inst1, inst2);

    inst1.type_option().goal = 75;
    EXPECT_EQ(inst1.get_coverage(), 100);
    inst1.type_option().goal = 80;
    EXPECT_EQ(inst1.get_coverage(), 75);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(merge_instances, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    merge_instances_cvp_only inst1("inst1", __FILE__, __LINE__, cntxt);
    merge_instances_cvp_only inst2("inst2", __FILE__, __LINE__, cntxt);
    inst1.type_option().merge_instances = true;
    sample_two_instances(inst1, inst2);

    xml_printer::coverage_save("merge_instances_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("merge_instances_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  // both instances are written, each with the type option
  size_t first = xml.find("merge_instances=\"true\"");
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(xml.find("merge_instances=\"true\"", first + 1), std::string::npos);
  EXPECT_EQ(xml.find("merge_instances=\"false\""), std::string::npos);
}
