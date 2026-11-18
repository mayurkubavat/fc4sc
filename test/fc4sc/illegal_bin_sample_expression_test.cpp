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

#include "fc4sc.hpp"
#include "gtest/gtest.h"

// Covergroup with an illegal bin in a coverpoint that is sampled through
// its sample expression.
class cvg_illegal_expression_test : public covergroup {
public:

  CG_CONS(cvg_illegal_expression_test) {
  };

  int value = 0;

  COVERPOINT(int, cvp1, value) {
    bin<int>("1", 1),
    illegal_bin<int>("illegal_2", 2)
  };

};

// The exception raised for an illegal sample has to keep its type on the way
// out of covergroup::sample(), unless FC4SC_NO_THROW is defined.
TEST(illegal_bin, sample_expression_throws) {
  auto cntxt = fc4sc::global::create_new_context();
  cvg_illegal_expression_test cvg("cvg",__FILE__,__LINE__,cntxt);
  EXPECT_EQ(cvg.get_inst_coverage(), 0);

  cvg.value = 1;
  cvg.sample();
  EXPECT_EQ(cvg.get_inst_coverage(), 100);

  cvg.value = 2;
#ifdef FC4SC_NO_THROW
  EXPECT_NO_THROW(cvg.sample());
#else
  EXPECT_THROW(cvg.sample(), fc4sc::illegal_bin_sample_exception);
#endif

  EXPECT_EQ(cvg.get_inst_coverage(), 100);
  fc4sc::global::delete_context(cntxt);
}
