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
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

// In SystemVerilog, a cross counts every combination of the bins of its
// coverpoints which are hit by a sample, not one bin of each coverpoint.

class cross_all_hits_cvg : public covergroup {
public:
  CG_CONS(cross_all_hits_cvg) {};

  int a = 0;
  int b = 0;

  void sample(int a, int b) {
    this->a = a;
    this->b = b;
    covergroup::sample();
  }

  // bins lo = {[0:5]}; bins hi = {[3:8]};
  COVERPOINT(int, cvp_a, a) {
    bin<int>("lo", interval(0, 5)),
    bin<int>("hi", interval(3, 8))
  };

  // bins one = {1}; bins small = {[0:2]};
  COVERPOINT(int, cvp_b, b) {
    bin<int>("one", 1),
    bin<int>("small", interval(0, 2))
  };

  // bins two = {2}; bins up = (1 => 2);
  COVERPOINT(int, cvp_t, a) {
    bin<int>("two", 2),
    transition_bin<int>("up", trans<int>(1, 2))
  };

  cross<int, int> a_x_b = cross<int, int>(this, "a_x_b", &cvp_a, &cvp_b);
  cross<int, int> t_x_b = cross<int, int>(this, "t_x_b", &cvp_t, &cvp_b);
};

typedef std::map<std::vector<size_t>, uint64_t> cross_bins_t;

// Index of a bin of a coverpoint, by name
static size_t bin_index(fc4sc::coverpoint_base& cvp, const std::string& name)
{
  auto bins = cvp.get_bins_base();
  for (size_t i = 0; i < bins.size(); ++i)
    if (bins[i]->name() == name) return i;
  throw std::invalid_argument("no bin " + name);
}

// Key of a cross bin: the bin of the first coverpoint given and the bin of
// the second one, in the order of the coverpoints in the cross
static std::vector<size_t> key(fc4sc::cross_base& crs,
                               fc4sc::coverpoint_base& cvp1, const std::string& bin1,
                               fc4sc::coverpoint_base& cvp2, const std::string& bin2)
{
  bool in_order = (crs.get_cross_coverpoints()[0] == &cvp1);
  size_t i1 = bin_index(cvp1, bin1);
  size_t i2 = bin_index(cvp2, bin2);
  return in_order ? std::vector<size_t>{i1, i2} : std::vector<size_t>{i2, i1};
}

TEST(cross_all_hit_bins, overlapping_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_all_hits_cvg cvg("cvg", __FILE__, __LINE__, cntxt);

    // a = 4 is in lo and hi, b = 1 is in one and small: 4 cross bins
    cvg.sample(4, 1);
    auto& x = cvg.a_x_b;
    auto& a = cvg.cvp_a;
    auto& b = cvg.cvp_b;
    EXPECT_EQ(x.get_cross_bins(),
              (cross_bins_t{{key(x, a, "lo", b, "one"), 1}, {key(x, a, "lo", b, "small"), 1},
                            {key(x, a, "hi", b, "one"), 1}, {key(x, a, "hi", b, "small"), 1}}));
    EXPECT_EQ(cvg.a_x_b.get_inst_coverage(), 100);

    // a = 0 is in lo only, b = 2 in small only
    cvg.sample(0, 2);
    EXPECT_EQ(x.get_cross_bins(),
              (cross_bins_t{{key(x, a, "lo", b, "one"), 1}, {key(x, a, "lo", b, "small"), 2},
                            {key(x, a, "hi", b, "one"), 1}, {key(x, a, "hi", b, "small"), 1}}));
    EXPECT_EQ(cvg.a_x_b.get_misses(), 0u);

    // no bin of cvp_b is hit: a miss of the cross
    cvg.sample(4, 9);
    EXPECT_EQ(cvg.a_x_b.get_misses(), 1u);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(cross_all_hit_bins, value_and_transition_bin) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cross_all_hits_cvg cvg("cvg", __FILE__, __LINE__, cntxt);

    // 1 then 2: the value 2 hits two and up of cvp_t
    cvg.sample(1, 0);
    cvg.sample(2, 0);
    auto& x = cvg.t_x_b;
    auto& t = cvg.cvp_t;
    auto& b = cvg.cvp_b;
    EXPECT_EQ(t.get_bin_hit_count(0), 1u);
    EXPECT_EQ(t.get_bin_hit_count(1), 1u);
    EXPECT_EQ(x.get_cross_bins(),
              (cross_bins_t{{key(x, t, "two", b, "small"), 1}, {key(x, t, "up", b, "small"), 1}}));

    // 2 again: two only
    cvg.sample(2, 1);
    EXPECT_EQ(x.get_cross_bins(),
              (cross_bins_t{{key(x, t, "two", b, "one"), 1}, {key(x, t, "two", b, "small"), 2},
                            {key(x, t, "up", b, "small"), 1}}));
  }
  fc4sc::global::delete_context(cntxt);
}
