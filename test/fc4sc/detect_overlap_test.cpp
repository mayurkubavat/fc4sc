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

/*
 * Tests for the detect_overlap option of coverpoints.
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

class overlap_cg : public covergroup {
public:

  CG_CONS(overlap_cg) {};

  int SAMPLE_POINT(sample_v, cvp);

  void sample(int v) {
    sample_v = v;
    covergroup::sample();
  }

  coverpoint<int> cvp = coverpoint<int> (this, "cvp",
    bin<int>("low", interval(0, 5)),
    bin<int>("high", interval(3, 8)),
    bin<int>("apart", interval(10, 12)),
    illegal_bin<int>("bad", 4)
  );
};

std::string read_file(const std::string &name) {
  std::ifstream in(name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string test_name() {
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

} // namespace

TEST(detect_overlap, warns_for_overlapping_bins) {
  auto cntxt = fc4sc::global::create_new_context();
  overlap_cg cg("cg", __FILE__, __LINE__, cntxt);
  cg.cvp.option().detect_overlap = true;

  ::testing::internal::CaptureStderr();
  cg.sample(1);
  cg.sample(7);
  std::string err = ::testing::internal::GetCapturedStderr();

  // one warning, for the one pair of regular bins which overlap
  const bool low_high = err.find("bins low and high of coverpoint cvp overlap") != std::string::npos;
  const bool high_low = err.find("bins high and low of coverpoint cvp overlap") != std::string::npos;
  EXPECT_TRUE(low_high || high_low) << err;
  EXPECT_EQ(err.find("Warning"), err.rfind("Warning")) << err;
  EXPECT_EQ(err.find("apart"), std::string::npos) << err;
  EXPECT_EQ(err.find("bad"), std::string::npos) << err;

  // the option does not change the sampling
  EXPECT_EQ(cg.cvp.get_bin_hit_count(0) + cg.cvp.get_bin_hit_count(1) + cg.cvp.get_bin_hit_count(2), 2u);

  const std::string file = "detect_overlap_" + test_name() + ".xml";
  xml_printer::coverage_save(file, cntxt);
  EXPECT_NE(read_file(file).find("detect_overlap=\"1\""), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}

TEST(detect_overlap, silent_by_default) {
  auto cntxt = fc4sc::global::create_new_context();
  overlap_cg cg("cg", __FILE__, __LINE__, cntxt);

  ::testing::internal::CaptureStderr();
  cg.sample(5);
  std::string err = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(err.find("overlap"), std::string::npos) << err;

  fc4sc::global::delete_context(cntxt);
}

TEST(detect_overlap, warns_when_set_after_sampling) {
  auto cntxt = fc4sc::global::create_new_context();
  overlap_cg cg("cg", __FILE__, __LINE__, cntxt);

  ::testing::internal::CaptureStderr();
  cg.sample(1);
  cg.cvp.option().detect_overlap = true;
  cg.sample(1);
  cg.sample(1);
  std::string err = ::testing::internal::GetCapturedStderr();

  // checked on the first sample after the option is set, and only once
  EXPECT_NE(err.find("Warning"), std::string::npos) << err;
  EXPECT_EQ(err.find("Warning"), err.rfind("Warning")) << err;

  fc4sc::global::delete_context(cntxt);
}

TEST(detect_overlap, warns_when_stopped) {
  auto cntxt = fc4sc::global::create_new_context();
  overlap_cg cg("cg", __FILE__, __LINE__, cntxt);
  cg.cvp.option().detect_overlap = true;
  cg.stop();

  ::testing::internal::CaptureStderr();
  cg.sample(1);
  std::string err = ::testing::internal::GetCapturedStderr();

  EXPECT_NE(err.find("overlap"), std::string::npos) << err;
  EXPECT_EQ(cg.cvp.get_bin_hit_count(0) + cg.cvp.get_bin_hit_count(1), 0u);

  fc4sc::global::delete_context(cntxt);
}
