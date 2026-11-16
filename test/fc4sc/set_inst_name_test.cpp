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
 * Tests for covergroup::set_inst_name.
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

class options_methods_cg : public covergroup {
public:

  CG_CONS(options_methods_cg) {};

  int SAMPLE_POINT(sample_a, cvp_a);
  int SAMPLE_POINT(sample_b, cvp_b);

  void sample(int a, int b) {
    sample_a = a;
    sample_b = b;
    covergroup::sample();
  }

  coverpoint<int> cvp_a = coverpoint<int> (this, "cvp_a",
    bin<int>("a0", 0),
    bin<int>("a1", 1),
    bin<int>("a2", 2),
    bin<int>("a3", 3)
  );

  coverpoint<int> cvp_b = coverpoint<int> (this, "cvp_b",
    bin<int>("b0", 0),
    bin<int>("b1", 1)
  );

  cross<int,int> crs = cross<int,int> (this, "crs", &cvp_a, &cvp_b);
};

std::string read_file(const std::string &name) {
  std::ifstream in(name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

int count(const std::string &text, const std::string &pattern) {
  int n = 0;
  for (auto pos = text.find(pattern); pos != std::string::npos; pos = text.find(pattern, pos + 1))
    ++n;
  return n;
}

std::string test_name() {
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

} // namespace

TEST(set_inst_name, renames_instance) {
  auto cntxt = fc4sc::global::create_new_context();
  options_methods_cg cg("before", __FILE__, __LINE__, cntxt);
  options_methods_cg other("other", __FILE__, __LINE__, cntxt);
  // sample every instance, so that every cross has a hit bin
  cg.sample(1, 1);
  other.sample(0, 0);

  cg.set_inst_name("after");
  EXPECT_EQ(cg.name(), "after");

  fc4sc::scp_base* scope = fc4sc::global::get_default_scope(cntxt);
  EXPECT_EQ(&scope->get_covergroup("after"), &cg);
  EXPECT_EQ(scope->cvg_insts.count("before"), 0u);
  EXPECT_EQ(scope->get_scp_data()->cvg_insts.count("before"), 0u);
  EXPECT_EQ(scope->get_scp_data()->cvg_insts.at("after"), cg.get_cvg_data());

  // the old name can be used again
  options_methods_cg reuse("before", __FILE__, __LINE__, cntxt);
  EXPECT_EQ(&scope->get_covergroup("before"), &reuse);
  reuse.sample(0, 0);

  // the name of another instance cannot be taken, and nothing changes
  // what is thrown is a C string, as in the rest of the library
  EXPECT_THROW(cg.set_inst_name("other"), const char*);
  EXPECT_THROW(cg.set_inst_name(""), const char*);
  EXPECT_EQ(cg.name(), "after");
  EXPECT_EQ(&scope->get_covergroup("other"), &other);

  // setting the current name again is allowed
  cg.set_inst_name("after");
  EXPECT_EQ(cg.name(), "after");

  // the coverage database has the new name
  const std::string file = "set_inst_name_" + test_name() + ".xml";
  xml_printer::coverage_save(file, cntxt);
  const std::string xml = read_file(file);
  EXPECT_EQ(count(xml, "<cgInstance name=\"after\""), 1);
  EXPECT_EQ(count(xml, "<cgInstance name=\"before\""), 1);
  EXPECT_EQ(count(xml, "<cgInstance name=\"other\""), 1);

  fc4sc::global::delete_context(cntxt);
}

TEST(set_inst_name, renames_instance_in_scope) {
  auto cntxt = fc4sc::global::create_new_context();
  options_methods_cg cg("", __FILE__, __LINE__, cntxt);
  const std::string anonymous = cg.name();
  EXPECT_FALSE(anonymous.empty());

  cg.set_inst_name("named");
  fc4sc::scp_base* scope = fc4sc::global::get_default_scope(cntxt);
  EXPECT_EQ(&scope->get_covergroup("named"), &cg);
  EXPECT_EQ(scope->cvg_insts.count(anonymous), 0u);

  fc4sc::global::delete_context(cntxt);
}
