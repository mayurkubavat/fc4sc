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
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

using fc4sc::goto_repeat;
using fc4sc::nonconsecutive_repeat;
using fc4sc::repeat;

typedef std::vector<std::vector<int>> sequences_t;

TEST(transition_repetition, zero_repetitions) {
  // (1 => 2 [*0:1] => 3)
  EXPECT_EQ(trans<int>(1, repeat(2, 0, 1), 3).sequences(), (sequences_t{{1, 3}, {1, 2, 3}}));
  // [*0] alone repeats nothing
  EXPECT_THROW(trans<int>(1, repeat(2, 0), 3), std::invalid_argument);
  EXPECT_EQ(trans<int>(repeat(1, 0, 2), 3).sequences(), (sequences_t{{3}, {1, 3}, {1, 1, 3}}));
  // a transition must match at least one value
  EXPECT_THROW(trans<int>(repeat(1, 0)), std::invalid_argument);
  EXPECT_THROW(trans<int>(repeat(1, 0, 2), repeat(3, 0, 1)), std::invalid_argument);
  EXPECT_THROW(trans<int>(repeat(2, 3, 2)), std::invalid_argument);
  EXPECT_THROW(trans<int>(goto_repeat(2, 0)), std::invalid_argument);
  EXPECT_THROW(trans<int>(nonconsecutive_repeat(2, 0, 1)), std::invalid_argument);
  EXPECT_THROW(trans<int>(goto_repeat(2, 3, 2)), std::invalid_argument);
}

TEST(transition_repetition, patterns) {
  EXPECT_FALSE(trans<int>(1, repeat(2, 2, 3)).is_pattern());
  EXPECT_TRUE(trans<int>(1, goto_repeat(3, 2), 5).is_pattern());
  EXPECT_THROW(trans<int>(1, goto_repeat(3, 2), 5).sequences(), std::invalid_argument);
  EXPECT_EQ(trans<int>(1, goto_repeat(3, 2), 5).text(), "1 => 3 [->2] => 5");
  EXPECT_EQ(trans<int>(std::vector<int>{1, 2}, nonconsecutive_repeat(interval(3, 4), 1, 2),
                       repeat(5, 0, 1)).text(),
            "1,2 => [3:4] [=1:2] => 5 [*0:1]");
  EXPECT_EQ(trans<int>(1, goto_repeat(3, 2), 5).pattern().shortest, (std::vector<int>{1, 3, 3, 5}));
  // the automaton is bounded
  EXPECT_THROW(trans<int>(goto_repeat(1, 5000)).pattern(), std::invalid_argument);
  EXPECT_THROW(transition_bin<int>("b", trans<int>(goto_repeat(1, 1, uint64_t(1) << 40))),
               std::invalid_argument);
  EXPECT_THROW(trans<int>(goto_repeat(1, 1000, 1500), 2, nonconsecutive_repeat(2, 1000)).pattern(),
               std::invalid_argument);
}

class repetition_cvg : public covergroup {
public:
  CG_CONS(repetition_cvg) {};

  int x = 0;
  int y = 7;

  void sample(std::initializer_list<int> values) {
    for (int v : values) {
      this->x = v;
      covergroup::sample();
    }
  }

  // bins b = (1 => 3 [-> 2] => 5);
  COVERPOINT(int, go, x) {
    transition_bin<int>("b", trans<int>(1, goto_repeat(3, 2), 5))
  };

  // bins b = (3 [-> 1:2] => 5);
  COVERPOINT(int, go_range, x) {
    transition_bin<int>("b", trans<int>(goto_repeat(3, 1, 2), 5))
  };

  // bins b = (1 => 3 [= 2] => 5);
  COVERPOINT(int, noncons, x) {
    transition_bin<int>("b", trans<int>(1, nonconsecutive_repeat(3, 2), 5))
  };

  // bins b = (2 [= 2]);
  COVERPOINT(int, noncons_end, x) {
    transition_bin<int>("b", trans<int>(nonconsecutive_repeat(2, 2)))
  };

  // bins b = (4 => 5), (5 [-> 1]);
  // bins arr[] = (4 => 5), (6 [-> 2]);
  COVERPOINT(int, mixed, x) {
    transition_bin<int>("b", trans<int>(4, 5), trans<int>(goto_repeat(5, 1))),
    transition_bin_array<int>("arr", trans<int>(4, 5), trans<int>(goto_repeat(6, 2)))
  };

  // bins b = (1 => 3 [-> 1]);
  // ignore_bins zero = {0};
  // ignore_bins ign = (8 => 9 [-> 1]);
  // bins c = (8 => 9 [-> 1]);
  // illegal_bins bad = (11 => 12 [= 2] => 13);
  // bins others = default sequence;
  COVERPOINT(int, special, x) {
    transition_bin<int>("b", trans<int>(1, goto_repeat(3, 1))),
    ignore_bin<int>("zero", 0),
    ignore_transition_bin<int>("ign", trans<int>(8, goto_repeat(9, 1))),
    transition_bin<int>("c", trans<int>(8, goto_repeat(9, 1))),
    illegal_transition_bin<int>("bad", trans<int>(11, nonconsecutive_repeat(12, 2), 13)),
    default_sequence_bin<int>("others")
  };

  // bins one = {1}; bins others = default sequence;
  COVERPOINT(int, no_trans, x) {
    bin<int>("one", 1),
    default_sequence_bin<int>("others")
  };

  COVERPOINT(int, other, y) {
    bin<int>("seven", 7)
  };

  // bins three = binsof(go) intersect {3};
  cross<int, int> go_x_other = cross<int, int>(this, "go_x_other", &go, &other,
    cross_bin("three", binsof<int>(&go).intersect(3)));
};

static uint64_t sequence_hits(fc4sc::coverpoint_base& cvp, size_t bin, size_t seq) {
  auto data = static_cast<fc4sc::coverpoint_base_data_model*>(cvp.get_data());
  return data->bins_data.at(bin)->get_interval_hits().at(seq);
}

static uint64_t default_hits(fc4sc::coverpoint_base& cvp) {
  auto data = static_cast<fc4sc::coverpoint_base_data_model*>(cvp.get_data());
  return data->default_bins_data.at(0)->get_interval_hits().at(0);
}

TEST(transition_repetition, goto_repetition) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({1, 3, 4, 3, 5});
    EXPECT_EQ(cvg.go.get_bin_hit_count(0), 1u);
    cvg.sample({1, 3, 3, 5});
    EXPECT_EQ(cvg.go.get_bin_hit_count(0), 2u);
    // other values before the first 3
    cvg.sample({1, 2, 3, 2, 3, 5});
    EXPECT_EQ(cvg.go.get_bin_hit_count(0), 3u);
    // 5 must follow the second 3
    cvg.sample({1, 3, 4, 3, 4, 5});
    EXPECT_EQ(cvg.go.get_bin_hit_count(0), 3u);
    // one 3 only, or three
    cvg.sample({1, 3, 5, 1, 3, 3, 3, 5});
    EXPECT_EQ(cvg.go.get_bin_hit_count(0), 3u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, goto_repetition_range) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({3, 5});
    EXPECT_EQ(cvg.go_range.get_bin_hit_count(0), 1u);
    cvg.sample({3, 4, 5});
    EXPECT_EQ(cvg.go_range.get_bin_hit_count(0), 1u);
    // 3 => 5 and 3 => 3 => 5 end with the same sample: one hit
    cvg.sample({3, 3, 5});
    EXPECT_EQ(cvg.go_range.get_bin_hit_count(0), 2u);
    cvg.sample({4, 3, 4, 4, 3, 5});
    EXPECT_EQ(cvg.go_range.get_bin_hit_count(0), 3u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, nonconsecutive_repetition) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // other values after the second 3
    cvg.sample({1, 3, 4, 3, 4, 4, 5});
    EXPECT_EQ(cvg.noncons.get_bin_hit_count(0), 1u);
    cvg.sample({1, 3, 3, 5});
    EXPECT_EQ(cvg.noncons.get_bin_hit_count(0), 2u);
    cvg.sample({1, 3, 5, 1, 3, 3, 3, 5});
    EXPECT_EQ(cvg.noncons.get_bin_hit_count(0), 2u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, nonconsecutive_repetition_at_the_end) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // (2 [= 2]) ends with 2 => 1 => 2, 2 => 1 => 2 => 1 and 2 => 1 => 2 => 1 => 1
    cvg.sample({2, 1, 2, 1, 1});
    EXPECT_EQ(cvg.noncons_end.get_bin_hit_count(0), 3u);
    // a third 2 ends 2 => 2 only
    cvg.sample({2});
    EXPECT_EQ(cvg.noncons_end.get_bin_hit_count(0), 4u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, shortest_match_and_arrays) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // 4 => 5 and 5 [-> 1] end with the same sample: the shortest is counted
    cvg.sample({4, 5});
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(0), 1u);
    EXPECT_EQ(sequence_hits(cvg.mixed, 0, 0), 0u);
    EXPECT_EQ(sequence_hits(cvg.mixed, 0, 1), 1u);
    // arr[0] = (4 => 5), arr[1] = (6 [-> 2])
    ASSERT_EQ(cvg.mixed.size(), 3u);
    EXPECT_EQ(cvg.mixed.get_bins_base()[2]->name(), "arr[1]");
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(1), 1u);
    cvg.sample({6, 1, 2, 6});
    EXPECT_EQ(cvg.mixed.get_bin_hit_count(2), 1u);
    EXPECT_EQ(cvg.mixed.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, ignored_values_and_transitions) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // no transition goes on past an ignored value
    cvg.sample({1, 0, 3});
    EXPECT_EQ(cvg.special.get_bin_hit_count(0), 0u);
    cvg.sample({1, 2, 3});
    EXPECT_EQ(cvg.special.get_bin_hit_count(0), 1u);
    // 8 => 9 [-> 1] is ignored
    cvg.sample({8, 2, 9});
    EXPECT_EQ(cvg.special.get_bin_hit_count(1), 0u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, illegal) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({11, 12, 1, 12, 1});
    try {
      cvg.sample({13});
      FAIL() << "the illegal transition was not detected";
    }
    catch (fc4sc::illegal_bin_sample_exception &e) {
      EXPECT_NE(std::string(e.what()).find("11 => 12 [=2] => 13"), std::string::npos) << e.what();
    }
    // 11 => 12 => 13 is not illegal
    cvg.sample({11, 12, 13});
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, default_sequence) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // 1 and 2 end no transition, 3 ends 1 => 2 => 3 [-> 1]
    cvg.sample({1, 2, 3});
    EXPECT_EQ(default_hits(cvg.special), 2u);
    // an ignored value is not counted, and 8 => 9 is an ignored transition
    cvg.sample({0, 8, 9});
    EXPECT_EQ(default_hits(cvg.special), 3u);
    // the default sequence bin is not in the coverage: b and c, b hit
    EXPECT_EQ(cvg.special.size(), 2u);
    EXPECT_EQ(cvg.special.get_inst_coverage(), 50);

    // without transition bins, each sample is counted
    EXPECT_EQ(default_hits(cvg.no_trans), 6u);
    EXPECT_EQ(cvg.no_trans.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, cross_select) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    auto data = static_cast<fc4sc::cross_base_data_model*>(cvg.go_x_other.get_data());
    ASSERT_EQ(data->user_bins.size(), 1u);
    // the value 3 of the goto repetition is a value of the bin
    EXPECT_EQ(data->user_bins[0].size, 1u);
    cvg.sample({1, 3, 3, 5});
    EXPECT_EQ(data->user_bins[0].hits, 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(transition_repetition, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    repetition_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample({1, 3, 3, 5, 4, 5});
    xml_printer::coverage_save("transition_repetition_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("transition_repetition_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();
  // the goto repetition is written as its shortest sequence
  size_t pos = xml.find("<coverpoint name=\"go\"");
  ASSERT_NE(pos, std::string::npos);
  pos = xml.find("<coverpointBin name=\"b\"", pos);
  const std::string b = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(b.find("<sequence>\n<contents coverageCount=\"1\"></contents>\n"
                   "<seqValue>1</seqValue>\n<seqValue>3</seqValue>\n<seqValue>3</seqValue>\n"
                   "<seqValue>5</seqValue>"),
            std::string::npos) << b;
  // the default sequence bin is an ignore bin of all the values
  pos = xml.find("<coverpoint name=\"no_trans\"");
  pos = xml.find("<coverpointBin name=\"others\"", pos);
  ASSERT_NE(pos, std::string::npos);
  const std::string others = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(others.find("type=\"ignore\""), std::string::npos);
  EXPECT_NE(others.find("from=\"-2147483648\""), std::string::npos);
  EXPECT_NE(others.find("coverageCount=\"6\""), std::string::npos);
}

class random_cvg : public covergroup {
public:
  CG_CONS(random_cvg) {};

  int x = 0;

  COVERPOINT(int, a, x) {
    transition_bin<int>("b", trans<int>(1, goto_repeat(3, 2), 4))
  };
  COVERPOINT(int, b, x) {
    transition_bin<int>("b", trans<int>(nonconsecutive_repeat(2, 1, 2), 0))
  };
  COVERPOINT(int, c, x) {
    transition_bin<int>("b", trans<int>(1, repeat(2, 0, 2), goto_repeat(std::vector<int>{3, 4}, 1)))
  };
  COVERPOINT(int, d, x) {
    transition_bin<int>("b", trans<int>(goto_repeat(3, 1, 3)))
  };
};

// The same transitions as regular expressions of the values 0 to 4,
// written as the letters a to e
static const char* random_patterns[] = {
  "b(?:[^d]*d){2}e",
  "(?:[^c]*c){1,2}[^c]*a",
  "bc{0,2}(?:[^de]*[de]){1}",
  "(?:[^d]*d){1,3}"
};

TEST(transition_repetition, matches_a_regular_expression) {
  std::mt19937 gen(12345);
  std::uniform_int_distribution<int> dist(0, 4);
  std::vector<int> values(300);
  for (auto &v : values) v = dist(gen);
  std::string text;
  for (auto v : values) text += static_cast<char>('a' + v);

  auto cntxt = fc4sc::global::create_new_context();
  {
    random_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    for (auto v : values) {
      cvg.x = v;
      cvg.sample();
    }
    fc4sc::coverpoint<int>* cvps[] = {&cvg.a, &cvg.b, &cvg.c, &cvg.d};
    for (size_t i = 0; i < 4; ++i) {
      // the samples which end a match
      std::regex re(random_patterns[i]);
      uint64_t expected = 0;
      for (size_t end = 0; end < text.size(); ++end) {
        for (size_t start = 0; start <= end; ++start) {
          if (std::regex_match(text.substr(start, end - start + 1), re)) {
            expected++;
            break;
          }
        }
      }
      EXPECT_GT(expected, 0u);
      EXPECT_EQ(cvps[i]->get_bin_hit_count(0), expected) << random_patterns[i];
    }
  }
  fc4sc::global::delete_context(cntxt);
}
