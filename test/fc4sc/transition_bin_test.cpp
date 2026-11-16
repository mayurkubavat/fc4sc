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
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

typedef std::vector<std::vector<int>> sequences_t;

TEST(transition_bin, sequences) {
  EXPECT_EQ(trans<int>(1, 2, 3).sequences(), (sequences_t{{1, 2, 3}}));
  EXPECT_EQ(trans<int>(4).sequences(), (sequences_t{{4}}));
  // (1,5 => [6:7])
  EXPECT_EQ(trans<int>(std::vector<int>{1, 5}, interval(6, 7)).sequences(),
            (sequences_t{{1, 6}, {1, 7}, {5, 6}, {5, 7}}));
  EXPECT_EQ(trans<int>(std::vector<fc4sc::interval_t<int>>{{1, 2}, {9, 9}}).sequences(),
            (sequences_t{{1}, {2}, {9}}));
  // (3 [*3]) and (1 => 2 [*2:3] => 4)
  EXPECT_EQ(trans<int>(fc4sc::repeat(3, 3)).sequences(), (sequences_t{{3, 3, 3}}));
  EXPECT_EQ(trans<int>(1, fc4sc::repeat(2, 2, 3), 4).sequences(),
            (sequences_t{{1, 2, 2, 4}, {1, 2, 2, 2, 4}}));
  EXPECT_EQ(trans<int>(fc4sc::repeat(std::vector<int>{1, 2}, 2)).sequences(),
            (sequences_t{{1, 1}, {1, 2}, {2, 1}, {2, 2}}));
}

TEST(transition_bin, bad_transitions) {
  EXPECT_THROW(trans<int>(), std::invalid_argument);
  EXPECT_THROW(trans<int>(1, fc4sc::repeat(2, 0)), std::invalid_argument);
  EXPECT_THROW(trans<int>(1, fc4sc::repeat(2, 3, 2)), std::invalid_argument);
  // 256 * 256 * 2 sequences
  EXPECT_THROW(trans<int>(interval(0, 255), interval(0, 255), interval(0, 1)).sequences(),
               std::invalid_argument);
  EXPECT_THROW(trans<int>(interval(0, 0x7fffffff)).sequences(), std::invalid_argument);
  EXPECT_THROW(transition_bin<int>("b", trans<int>(interval(0, 255), interval(0, 255)),
                                   trans<int>(1, 2)),
               std::invalid_argument);
}

class transition_cvg : public covergroup {
public:
  CG_CONS(transition_cvg) {};

  int x = 0;
  int y = 0;
  bool enable = true;

  void sample(std::initializer_list<int> values) {
    for (int v : values) {
      this->x = v;
      covergroup::sample();
    }
  }

  // SystemVerilog:
  //   bins seq = (1 => 2 => 3);
  //   bins set = (1,2 => 3,4);
  //   bins two = (5 => 6), (0 => 5 => 6);
  //   bins rep = (7 [*3]);
  COVERPOINT(int, single, x, enable) {
    transition_bin<int>("seq", trans<int>(1, 2, 3)),
    transition_bin<int>("set", trans<int>(std::vector<int>{1, 2}, std::vector<int>{3, 4})),
    transition_bin<int>("two", trans<int>(5, 6), trans<int>(0, 5, 6)),
    transition_bin<int>("rep", trans<int>(fc4sc::repeat(7, 3)))
  };

  // bins arr[] = (1,2 => 3,4);
  COVERPOINT(int, array, x) {
    transition_bin_array<int>("arr", trans<int>(std::vector<int>{1, 2}, std::vector<int>{3, 4}))
  };

  // bins one = {1}; bins up = (1 => 2); ignore_bins ign = (0 => 1 => 2);
  // illegal_bins bad = (9 => 9);
  COVERPOINT(int, mixed, x) {
    bin<int>("one", 1),
    transition_bin<int>("up", trans<int>(1, 2)),
    ignore_transition_bin<int>("ign", trans<int>(0, 1, 2)),
    illegal_transition_bin<int>("bad", trans<int>(9, 9))
  };

  COVERPOINT(int, other, y) {
    bin<int>("seven", 7)
  };

  cross<int, int> up_x_other = cross<int, int>(this, "up_x_other", &mixed, &other);
};

TEST(transition_bin, single_sequence) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({1, 2, 4});
    EXPECT_EQ(cvg.single.get_bin_hit_count(0), 0u);
    cvg.sample({1, 2, 1, 2, 3});
    EXPECT_EQ(cvg.single.get_bin_hit_count(0), 1u);
    cvg.sample({7, 7, 7, 7});
    EXPECT_EQ(cvg.single.get_bin_hit_count(3), 2u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, set_and_array) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({1, 3, 2, 4});

    // one bin for all 4 sequences, hit by 1 => 3 and 2 => 4
    EXPECT_EQ(cvg.single.get_bin_hit_count(1), 2u);

    // one bin for each sequence: 1=>3, 1=>4, 2=>3, 2=>4
    ASSERT_EQ(cvg.array.size(), 4u);
    EXPECT_EQ(cvg.array.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.array.get_bin_hit_count(1), 0u);
    EXPECT_EQ(cvg.array.get_bin_hit_count(2), 0u);
    EXPECT_EQ(cvg.array.get_bin_hit_count(3), 1u);
    EXPECT_EQ(cvg.array.get_inst_coverage(), 50);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, hit_once_per_sample) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // both 5 => 6 and 0 => 5 => 6 end here
    cvg.sample({0, 5, 6});
    EXPECT_EQ(cvg.single.get_bin_hit_count(2), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, sample_condition) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // a sample which is not taken is not part of the transition
    cvg.sample({1});
    cvg.enable = false;
    cvg.sample({9});
    cvg.enable = true;
    cvg.sample({3});
    EXPECT_EQ(cvg.single.get_bin_hit_count(1), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, mixed_with_value_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cvg.mixed.size(), 2u);
    cvg.sample({1});
    EXPECT_EQ(cvg.mixed.get_inst_coverage(), 50);
    cvg.sample({2});
    EXPECT_EQ(cvg.mixed.get_inst_coverage(), 100);
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(1), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, ignore) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({0, 1, 2});
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(1), 0u);
    cvg.sample({5, 1, 2});
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(1), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, illegal) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({9});
    try {
      cvg.sample({9});
      FAIL() << "illegal transition did not throw";
    }
    catch (fc4sc::illegal_bin_sample_exception &e) {
      EXPECT_NE(std::string(e.what()).find("mixed/bad] on value [9 => 9]"), std::string::npos)
          << e.what();
    }
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, cross) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.y = 7;
    cvg.sample({3, 2});
    EXPECT_TRUE(cvg.up_x_other.get_cross_bins().empty());
    cvg.sample({1, 2});
    // <one, seven> on 1, then <up, seven> on 2
    const auto& cvps = cvg.up_x_other.get_cross_coverpoints();
    std::vector<size_t> one_key(2, 0), up_key(2, 0);
    up_key[(cvps[0] == &cvg.mixed) ? 0 : 1] = 1;
    auto bins = cvg.up_x_other.get_cross_bins();
    EXPECT_EQ(bins.size(), 2u);
    EXPECT_EQ(bins[one_key], 1u);
    EXPECT_EQ(bins[up_key], 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_bin, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.y = 7;
    cvg.sample({1, 3, 2, 4, 1, 3});
    xml_printer::coverage_save("transition_bin_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("transition_bin_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  // the bin "set" has a sequence element for each of its 4 sequences
  size_t pos = xml.find("<coverpointBin name=\"set\"");
  ASSERT_NE(pos, std::string::npos);
  const std::string set = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_EQ(set.find("<range"), std::string::npos);
  const char *expected[] = {
    "<sequence>\n<contents coverageCount=\"2\"></contents>\n<seqValue>1</seqValue>\n<seqValue>3</seqValue>\n</sequence>",
    "<sequence>\n<contents coverageCount=\"0\"></contents>\n<seqValue>1</seqValue>\n<seqValue>4</seqValue>\n</sequence>",
    "<sequence>\n<contents coverageCount=\"0\"></contents>\n<seqValue>2</seqValue>\n<seqValue>3</seqValue>\n</sequence>",
    "<sequence>\n<contents coverageCount=\"1\"></contents>\n<seqValue>2</seqValue>\n<seqValue>4</seqValue>\n</sequence>",
  };
  size_t seq_pos = 0;
  for (auto e : expected) {
    seq_pos = set.find(e, seq_pos);
    EXPECT_NE(seq_pos, std::string::npos) << e;
  }

  EXPECT_NE(xml.find("<coverpointBin name=\"arr[3]\""), std::string::npos);
  pos = xml.find("<coverpointBin name=\"bad\"");
  ASSERT_NE(pos, std::string::npos);
  EXPECT_NE(xml.find("type=\"illegal\"", pos), std::string::npos);
  pos = xml.find("<coverpointBin name=\"ign\"");
  ASSERT_NE(pos, std::string::npos);
  EXPECT_NE(xml.find("type=\"ignore\"", pos), std::string::npos);
}

// A transition bin declared before the value bins of the same kind
class transition_first_cvg : public covergroup {
public:
  CG_CONS(transition_first_cvg) {};

  int x = 0;

  void sample(std::initializer_list<int> values) {
    for (int v : values) {
      this->x = v;
      covergroup::sample();
    }
  }

  COVERPOINT(int, regular, x) {
    transition_bin<int>("t12", trans<int>(1, 2)),
    bin<int>("v2", 2)
  };
  COVERPOINT(int, ignore, x) {
    bin<int>("v1", 1),
    ignore_transition_bin<int>("t78", trans<int>(7, 8)),
    ignore_bin<int>("v5", 5)
  };
  COVERPOINT(int, illegal, x) {
    bin<int>("v1", 1),
    illegal_transition_bin<int>("t78", trans<int>(7, 8)),
    illegal_bin<int>("v6", 6)
  };
};

TEST(transition_bin, declared_before_value_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_first_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({1, 2});
    EXPECT_EQ(cvg.regular.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cvg.regular.get_bin_hit_count(1), 1u);

    cvg.sample({5});
    EXPECT_EQ(cvg.ignore.get_misses(), 2u);
    EXPECT_EQ(cvg.ignore.get_ignore_bins_base()[1]->get_hitcount(), 1u);

    EXPECT_THROW(cvg.sample({6}), fc4sc::illegal_bin_sample_exception);
    EXPECT_EQ(cvg.illegal.get_illegal_bins_base()[1]->get_hitcount(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

class transition_ignore_value_cvg : public covergroup {
public:
  CG_CONS(transition_ignore_value_cvg) {};

  int x = 0;

  void sample(std::initializer_list<int> values) {
    for (int v : values) {
      this->x = v;
      covergroup::sample();
    }
  }

  // ignore_bins ign = {2,3};
  // bins t12 = (1 => 2); bins t134 = (1 => 3 => 4); bins t14 = (1 => 4);
  COVERPOINT(int, cvp, x) {
    ignore_bin<int>("ign", 2, 3),
    transition_bin<int>("t12", trans<int>(1, 2)),
    transition_bin<int>("t134", trans<int>(1, 3, 4)),
    transition_bin<int>("t14", trans<int>(1, 4))
  };
};

TEST(transition_bin, ignored_values) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    transition_ignore_value_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // a transition which ends on an ignored value is not hit, and the
    // sample is a miss only
    cvg.sample({1, 2});
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(0), 0u);
    EXPECT_EQ(cvg.cvp.get_misses(), 2u);
    // nor is a transition with an ignored value in it
    cvg.sample({1, 3, 4});
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(1), 0u);
    EXPECT_EQ(cvg.cvp.get_misses(), 5u);
    cvg.sample({1, 4});
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cvg.cvp.get_misses(), 6u);
  }
  fc4sc::global::delete_context(cntxt);
}
