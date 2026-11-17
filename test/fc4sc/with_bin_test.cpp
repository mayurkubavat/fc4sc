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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

typedef std::vector<fc4sc::interval_t<int>> intervals_t;

// the values of a set_covergroup_expression, as a function
static std::vector<int> primes_below(int limit) {
  std::vector<int> primes;
  for (int v = 2; v < limit; ++v) {
    bool prime = true;
    for (int d = 2; d * d <= v; ++d)
      if (v % d == 0) prime = false;
    if (prime) primes.push_back(v);
  }
  return primes;
}

static bool is_even(int v) { return v % 2 == 0; }

class with_cvg : public covergroup {
public:
  CG_CONS(with_cvg) {};

  int x = 0;

  void sample(int v) {
    this->x = v;
    covergroup::sample();
  }

  // bins even = {[0:15]} with (item % 2 == 0);
  // bins mid = {[0:20]} with (item >= 5 && item <= 9);
  // bins none = {[0:3]} with (item > 10);
  // bins primes = primes_below(20);
  // bins w = {4'b1???} with (item % 2 == 0);
  COVERPOINT(int, single, x) {
    bin<int>("even", interval(0, 15)).with(is_even),
    bin<int>("mid", interval(0, 20)).with([](int v) { return v >= 5 && v <= 9; }),
    bin<int>("none", interval(0, 3)).with([](int v) { return v > 10; }),
    bin<int>("primes", primes_below(20)),
    bin<int>("w", wildcard<int>("4'b1???")).with(is_even)
  };

  // bins b[] = {[0:15]} with (item % 3 == 0);
  COVERPOINT(int, each, x) {
    bin_array<int>("b", intervals_t{interval(0, 15)}).with([](int v) { return v % 3 == 0; })
  };

  // bins b[4] = {[0:15]} with (item % 3 == 0);
  COVERPOINT(int, four, x) {
    bin_array<int>("b", 4, interval(0, 15)).with([](int v) { return v % 3 == 0; })
  };

  // bins b[8] = {[0:15]} with (item % 5 == 0);
  COVERPOINT(int, eight, x) {
    bin_array<int>("b", 8, interval(0, 15)).with([](int v) { return v % 5 == 0; })
  };

  // bins b[2] = {1, 1, 2, 3};
  COVERPOINT(int, set_array, x) {
    bin_array<int>("b", 2, std::vector<int>{1, 1, 2, 3})
  };

  // bins all = {[0:15]};
  // ignore_bins quad = {[0:15]} with (item % 4 == 0);
  // illegal_bins odd = {[100:107]} with (item % 2 == 1);
  COVERPOINT(int, special, x) {
    bin<int>("all", interval(0, 15)),
    ignore_bin<int>("quad", interval(0, 15)).with([](int v) { return v % 4 == 0; }),
    illegal_bin<int>("odd", interval(100, 107)).with([](int v) { return v % 2 == 1; })
  };
};

static intervals_t bin_intervals(fc4sc::coverpoint_base& cvp, size_t index) {
  return cvp.get_bins_base().at(index)->get_intervals_to_int();
}

static std::string bin_name(fc4sc::coverpoint_base& cvp, size_t index) {
  return cvp.get_bins_base().at(index)->name();
}

TEST(with_bin, single_bin_values) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // "none" keeps no value and is not created
    ASSERT_EQ(cvg.single.size(), 4u);
    EXPECT_EQ(bin_name(cvg.single, 0), "even");
    EXPECT_EQ(bin_intervals(cvg.single, 0),
              (intervals_t{{0, 0}, {2, 2}, {4, 4}, {6, 6}, {8, 8}, {10, 10}, {12, 12}, {14, 14}}));
    // the values kept next to each other make one interval
    EXPECT_EQ(bin_name(cvg.single, 1), "mid");
    EXPECT_EQ(bin_intervals(cvg.single, 1), (intervals_t{{5, 9}}));
    // a value list has an interval for each value, as {2, 3}
    EXPECT_EQ(bin_name(cvg.single, 2), "primes");
    EXPECT_EQ(bin_intervals(cvg.single, 2),
              (intervals_t{{2, 2}, {3, 3}, {5, 5}, {7, 7}, {11, 11}, {13, 13}, {17, 17}, {19, 19}}));
    EXPECT_EQ(bin_intervals(cvg.single, 3),
              (intervals_t{{8, 8}, {10, 10}, {12, 12}, {14, 14}}));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(with_bin, single_bin_sampling) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(3);
    EXPECT_EQ(cvg.single.get_bin_hit_count(0), 0u);
    EXPECT_EQ(cvg.single.get_bin_hit_count(2), 1u);
    cvg.sample(4);
    cvg.sample(14);
    EXPECT_EQ(cvg.single.get_bin_hit_count(0), 2u);
    cvg.sample(7);
    EXPECT_EQ(cvg.single.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cvg.single.get_bin_hit_count(2), 2u);
    EXPECT_EQ(cvg.single.get_bin_hit_count(3), 1u);
    // 4 bins, all hit
    EXPECT_EQ(cvg.single.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(with_bin, bin_array_with_a_bin_for_each_value) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    ASSERT_EQ(cvg.each.size(), 6u);
    for (size_t i = 0; i < 6; ++i) {
      EXPECT_EQ(bin_name(cvg.each, i), "b[" + std::to_string(i) + "]");
      const int v = static_cast<int>(3 * i);
      EXPECT_EQ(bin_intervals(cvg.each, i), (intervals_t{{v, v}}));
    }
    cvg.sample(9);
    EXPECT_EQ(cvg.each.get_bin_hit_count(3), 1u);
    EXPECT_EQ(cvg.each.get_misses(), 0u);
    cvg.sample(10);
    EXPECT_EQ(cvg.each.get_misses(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(with_bin, bin_array_with_a_number_of_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    // 6 values in 4 bins: one value in each, the last bin has the rest
    ASSERT_EQ(cvg.four.size(), 4u);
    EXPECT_EQ(bin_intervals(cvg.four, 0), (intervals_t{{0, 0}}));
    EXPECT_EQ(bin_intervals(cvg.four, 1), (intervals_t{{3, 3}}));
    EXPECT_EQ(bin_intervals(cvg.four, 2), (intervals_t{{6, 6}}));
    EXPECT_EQ(bin_intervals(cvg.four, 3), (intervals_t{{9, 9}, {12, 12}, {15, 15}}));
    EXPECT_EQ(bin_name(cvg.four, 3), "b[3]");

    // 4 values in 8 bins: the bins left without values are not created
    ASSERT_EQ(cvg.eight.size(), 4u);
    EXPECT_EQ(bin_intervals(cvg.eight, 0), (intervals_t{{0, 0}}));
    EXPECT_EQ(bin_intervals(cvg.eight, 3), (intervals_t{{15, 15}}));

    // a value list in a number of bins, duplicate values are kept
    ASSERT_EQ(cvg.set_array.size(), 2u);
    EXPECT_EQ(bin_intervals(cvg.set_array, 0), (intervals_t{{1, 1}}));
    EXPECT_EQ(bin_intervals(cvg.set_array, 1), (intervals_t{{2, 3}}));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(with_bin, ignore_and_illegal_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(8);   // ignored
    EXPECT_EQ(cvg.special.get_bin_hit_count(0), 0u);
    cvg.sample(9);
    EXPECT_EQ(cvg.special.get_bin_hit_count(0), 1u);
    cvg.sample(102); // not illegal: even
    EXPECT_THROW(cvg.sample(103), fc4sc::illegal_bin_sample_exception);
    auto illegal = cvg.special.get_illegal_bins_base();
    ASSERT_EQ(illegal.size(), 1u);
    EXPECT_EQ(illegal[0]->get_intervals_to_int(),
              (intervals_t{{101, 101}, {103, 103}, {105, 105}, {107, 107}}));
    EXPECT_EQ(cvg.special.get_ignore_bins_base()[0]->get_intervals_to_int(),
              (intervals_t{{0, 0}, {4, 4}, {8, 8}, {12, 12}}));
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(with_bin, small_types) {
  // the values at the ends of a type are tested once, without overflow
  auto u8 = bin<unsigned char>("u8", interval<unsigned char>(0, 255))
              .with([](unsigned char v) { return v > 250; });
  EXPECT_EQ(u8.get_intervals_to_int(), (intervals_t{{251, 255}}));
  auto s8 = bin<signed char>("s8", interval<signed char>(-128, 127))
              .with([](signed char v) { return v < -126 || v == 127; });
  EXPECT_EQ(s8.get_intervals_to_int(), (intervals_t{{-128, -127}, {127, 127}}));
  auto b = bin<bool>("b", interval(false, true)).with([](bool v) { return v; });
  EXPECT_EQ(b.get_intervals_to_int(), (intervals_t{{1, 1}}));
  auto u64 = bin<unsigned long long>("u64", interval(ULLONG_MAX - 3, ULLONG_MAX))
               .with([](unsigned long long v) { return v % 2 == 1; });
  EXPECT_TRUE(u64.contains(ULLONG_MAX));
  EXPECT_FALSE(u64.contains(ULLONG_MAX - 1));
  EXPECT_TRUE(u64.contains(ULLONG_MAX - 2));
}

TEST(with_bin, bounded_number_of_values) {
  EXPECT_THROW(bin<int>("big", interval(0, INT_MAX)).with(is_even), std::invalid_argument);
  EXPECT_THROW(bin_array<int>("big", 4, interval(INT_MIN, INT_MAX)).with(is_even),
               std::invalid_argument);
  // 2^20 values are tested
  EXPECT_NO_THROW(bin<int>("max", interval(0, (1 << 20) - 1)).with(is_even));
  EXPECT_THROW(bin<int>("over", interval(0, 1 << 20)).with(is_even), std::invalid_argument);
  EXPECT_THROW(bin_array<int>("zero", 0, std::vector<int>{1}), std::invalid_argument);
}

TEST(with_bin, written_to_ucis_xml) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    with_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    cvg.sample(4);
    xml_printer::coverage_save("with_bin_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("with_bin_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();
  size_t pos = xml.find("<coverpointBin name=\"even\"");
  ASSERT_NE(pos, std::string::npos);
  const std::string even = xml.substr(pos, xml.find("</coverpointBin>", pos) - pos);
  EXPECT_NE(even.find("from=\"4\""), std::string::npos);
  EXPECT_EQ(even.find("from=\"5\""), std::string::npos);
  EXPECT_EQ(xml.find("<coverpointBin name=\"none\""), std::string::npos);
}

class empty_bin_cvg : public covergroup {
public:
  CG_CONS(empty_bin_cvg) {};

  int x = 0;

  COVERPOINT(int, cvp, x) {
    bin<int>("empty", intervals_t()),
    bin<int>("one", 1),
    ignore_bin<int>("empty_ignore", intervals_t()),
    illegal_bin<int>("empty_illegal", intervals_t())
  };
};

TEST(with_bin, empty_bins_are_not_added) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    empty_bin_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    ASSERT_EQ(cvg.cvp.size(), 1u);
    EXPECT_EQ(cvg.cvp.get_bins_base()[0]->name(), "one");
    EXPECT_TRUE(cvg.cvp.get_ignore_bins_base().empty());
    EXPECT_TRUE(cvg.cvp.get_illegal_bins_base().empty());
    cvg.x = 1;
    cvg.sample();
    EXPECT_EQ(cvg.cvp.get_inst_coverage(), 100);
  }
  fc4sc::global::delete_context(cntxt);
}

// bins b[4] = {[0:15]}; written with a braced pair, which converts both to
// an interval and to a vector of values
class braced_array_cvg : public covergroup {
public:
  CG_CONS(braced_array_cvg) {};

  int x = 0;

  COVERPOINT(int, cvp, x) {
    bin_array<int>("b", 4, {0, 15})
  };
};

TEST(with_bin, braced_pair_is_an_interval) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    braced_array_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    ASSERT_EQ(cvg.cvp.size(), 4u);
    EXPECT_EQ(bin_intervals(cvg.cvp, 0), (intervals_t{{0, 3}}));
    EXPECT_EQ(bin_intervals(cvg.cvp, 3), (intervals_t{{12, 15}}));
  }
  fc4sc::global::delete_context(cntxt);
}
