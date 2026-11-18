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
 * Tests that the names and comments which the user gives are escaped in the
 * UCIS XML file.
 */

#include "fc4sc.hpp"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

class xml_escape_cg : public covergroup {
public:

  CG_CONS(xml_escape_cg) {};

  int SAMPLE_POINT(sample_a, cvp_a);
  int SAMPLE_POINT(sample_b, cvp_b);

  void sample(int a, int b) {
    sample_a = a;
    sample_b = b;
    covergroup::sample();
  }

  coverpoint<int> cvp_a = coverpoint<int> (this, "cvp_a",
    bin<int>("a0", 0)
  );

  coverpoint<int> cvp_b = coverpoint<int> (this, "cvp_b",
    bin<int>("b0", 0)
  );

  cross<int,int> crs = cross<int,int> (this, "crs", &cvp_a, &cvp_b);
};

std::string read_file(const std::string &name) {
  std::ifstream in(name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

TEST(xml_escape, names_and_comments) {
  auto cntxt = fc4sc::global::create_new_context();
  xml_escape_cg cg("cg \"one\" & <two>", __FILE__, __LINE__, cntxt);
  cg.option().comment = "covergroup \"comment\" & <tag>";
  cg.cvp_a.option().comment = "coverpoint 'comment' & <tag>";
  cg.crs.option().comment = "cross \"comment\" & <tag>";
  cg.cvp_a.name() = "cvp<a>";
  cg.sample(0, 0);

  const std::string file = "xml_escape_names_and_comments.xml";
  xml_printer::coverage_save(file, cntxt);
  const std::string xml = read_file(file);

  EXPECT_NE(xml.find("<cgInstance name=\"cg &quot;one&quot; &amp; &lt;two&gt;\""), std::string::npos);
  EXPECT_NE(xml.find("comment=\"covergroup &quot;comment&quot; &amp; &lt;tag&gt;\""), std::string::npos);
  EXPECT_NE(xml.find("comment=\"coverpoint &apos;comment&apos; &amp; &lt;tag&gt;\""), std::string::npos);
  EXPECT_NE(xml.find("comment=\"cross &quot;comment&quot; &amp; &lt;tag&gt;\""), std::string::npos);
  EXPECT_NE(xml.find("<crossExpr>cvp&lt;a&gt;</crossExpr>"), std::string::npos);
  EXPECT_EQ(xml.find("<tag>"), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}

TEST(xml_escape, file_names_and_control_characters) {
  auto cntxt = fc4sc::global::create_new_context();
  xml_escape_cg cg("cg", "/src/R&D/\"cov\".cpp", 1, cntxt);
  cg.option().comment = std::string("ctl\x01" "char\tand\nlines");
  cg.sample(0, 0);

  const std::string file = "xml_escape_file_names_and_control_characters.xml";
  xml_printer::coverage_save(file, cntxt);
  const std::string xml = read_file(file);

  EXPECT_NE(xml.find("fileName=\"/src/R&amp;D/&quot;cov&quot;.cpp\""), std::string::npos);
  // characters XML 1.0 does not allow are replaced, white space is kept
  EXPECT_EQ(xml.find('\x01'), std::string::npos);
  EXPECT_NE(xml.find("comment=\"ctl?char&#9;and&#10;lines\""), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}

TEST(xml_escape, bytes_which_are_not_utf8) {
  auto cntxt = fc4sc::global::create_new_context();
  // Latin-1 bytes, an overlong encoding, an encoded surrogate, a character
  // cut short and U+FFFF, which are not allowed in the file; and UTF-8
  // characters of two, three and four bytes, which are kept
  xml_escape_cg cg("enc\xe9", __FILE__, __LINE__, cntxt);
  cg.option().comment = "comment \xff\xfe \xc0\xaf \xed\xa0\x80 \xef\xbf\xbf \xe2\x82";
  cg.cvp_a.option().comment = "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x99\x82";
  cg.sample(0, 0);

  const std::string file = "xml_escape_not_utf8.xml";
  xml_printer::coverage_save(file, cntxt);
  const std::string xml = read_file(file);

  EXPECT_NE(xml.find("<cgInstance name=\"enc?\""), std::string::npos);
  EXPECT_NE(xml.find("comment=\"comment ?? ?? ??? ??? ??\""), std::string::npos);
  EXPECT_NE(xml.find("comment=\"caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x99\x82\""), std::string::npos);
  EXPECT_EQ(xml.find('\xe9'), std::string::npos);
  EXPECT_EQ(xml.find('\xff'), std::string::npos);

  fc4sc::global::delete_context(cntxt);
}
