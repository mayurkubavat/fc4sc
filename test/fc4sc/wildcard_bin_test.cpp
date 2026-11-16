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
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

typedef std::vector<fc4sc::interval_t<uint8_t>> u8_intervals;
typedef std::vector<fc4sc::interval_t<int8_t>> s8_intervals;

TEST(wildcard_bin, binary_patterns) {
  EXPECT_EQ(wildcard<uint8_t>("1??0"),
            (u8_intervals{{8, 8}, {10, 10}, {12, 12}, {14, 14}}));
  EXPECT_EQ(wildcard<uint8_t>("11??"), (u8_intervals{{12, 15}}));
  EXPECT_EQ(wildcard<uint8_t>("1x0Z"), (u8_intervals{{8, 9}, {12, 13}}));
  EXPECT_EQ(wildcard<uint8_t>("10_01"), (u8_intervals{{9, 9}}));
  EXPECT_EQ(wildcard<uint8_t>("????????"), (u8_intervals{{0, 255}}));
}

TEST(wildcard_bin, literal_patterns) {
  EXPECT_EQ(wildcard<uint8_t>("4'b1??0"), wildcard<uint8_t>("1??0"));
  EXPECT_EQ(wildcard<uint8_t>("'h3?"), (u8_intervals{{0x30, 0x3f}}));
  EXPECT_EQ(wildcard<uint8_t>("8'hx5"),
            (u8_intervals{{0x05, 0x05}, {0x15, 0x15}, {0x25, 0x25}, {0x35, 0x35},
                          {0x45, 0x45}, {0x55, 0x55}, {0x65, 0x65}, {0x75, 0x75},
                          {0x85, 0x85}, {0x95, 0x95}, {0xa5, 0xa5}, {0xb5, 0xb5},
                          {0xc5, 0xc5}, {0xd5, 0xd5}, {0xe5, 0xe5}, {0xf5, 0xf5}}));
  EXPECT_EQ(wildcard<uint8_t>("'o7?"), (u8_intervals{{56, 63}}));
  // extended with zeros, or with don't care bits after a leftmost x
  EXPECT_EQ(wildcard<uint8_t>("6'b1?"), (u8_intervals{{2, 3}}));
  EXPECT_EQ(wildcard<uint8_t>("6'b?1"),
            (u8_intervals{{1, 1}, {3, 3}, {5, 5}, {7, 7}, {9, 9}, {11, 11},
                          {13, 13}, {15, 15}, {17, 17}, {19, 19}, {21, 21},
                          {23, 23}, {25, 25}, {27, 27}, {29, 29}, {31, 31},
                          {33, 33}, {35, 35}, {37, 37}, {39, 39}, {41, 41},
                          {43, 43}, {45, 45}, {47, 47}, {49, 49}, {51, 51},
                          {53, 53}, {55, 55}, {57, 57}, {59, 59}, {61, 61},
                          {63, 63}}));
}

TEST(wildcard_bin, signed_types) {
  EXPECT_EQ(wildcard<int8_t>("8'b1???????"), (s8_intervals{{-128, -1}}));
  EXPECT_EQ(wildcard<int8_t>("8'b????????"), (s8_intervals{{-128, 127}}));
  s8_intervals even_negative = wildcard<int8_t>("8'b1??????0");
  ASSERT_EQ(even_negative.size(), 64u);
  EXPECT_EQ(even_negative.front(), (fc4sc::interval_t<int8_t>{-128, -128}));
  EXPECT_EQ(even_negative.back(), (fc4sc::interval_t<int8_t>{-2, -2}));
  EXPECT_EQ(wildcard<int8_t>("8'b1111111?"), (s8_intervals{{-2, -1}}));
  EXPECT_EQ(wildcard<int8_t>("7'b???????"), (s8_intervals{{0, 127}}));
  EXPECT_EQ(wildcard<int64_t>("'h?"), (std::vector<fc4sc::interval_t<int64_t>>{{0, 15}}));
}

TEST(wildcard_bin, values) {
  EXPECT_EQ(wildcard_values<int>("4'b1?0?"), (std::vector<int>{8, 9, 12, 13}));
}

TEST(wildcard_bin, bad_patterns) {
  EXPECT_THROW(wildcard<uint8_t>(""), std::invalid_argument);
  EXPECT_THROW(wildcard<uint8_t>("4'b12"), std::invalid_argument);
  EXPECT_THROW(wildcard<uint8_t>("4'd1?"), std::invalid_argument);
  EXPECT_THROW(wildcard<uint8_t>("2'b1?1"), std::invalid_argument);
  EXPECT_THROW(wildcard<uint8_t>("9'b1????????"), std::invalid_argument);
  EXPECT_THROW(wildcard<uint8_t>("'hg"), std::invalid_argument);
  // 2^17 intervals: 17 don't care bits above a bit to match
  EXPECT_THROW(wildcard<uint32_t>("'b" + std::string(17, '?') + "0"), std::invalid_argument);
  EXPECT_EQ(wildcard<uint32_t>("'b" + std::string(16, '?') + "0").size(), 1u << 16);
  EXPECT_EQ(wildcard<uint32_t>("32'b1" + std::string(31, '?')),
            (std::vector<fc4sc::interval_t<uint32_t>>{{0x80000000u, 0xffffffffu}}));
  // zero bits above the type are allowed
  EXPECT_EQ(wildcard<uint8_t>("9'b01111111?"), (u8_intervals{{254, 255}}));
}

class wildcard_cvg : public covergroup {
public:
  CG_CONS(wildcard_cvg) {};

  int x = 0;

  void sample(int x) {
    this->x = x;
    covergroup::sample();
  }

  // SystemVerilog:
  //   wildcard bins high = { 4'b11?? };
  //   wildcard bins odd_low[] = { 4'b01?1 };
  //   wildcard ignore_bins ign = { 4'b00?? };
  //   wildcard illegal_bins ill = { 4'b10?1 };
  COVERPOINT(int, cvp, x) {
    bin<int>("high", wildcard<int>("4'b11??")),
    bin_array<int>("odd_low", wildcard_values<int>("4'b01?1")),
    ignore_bin<int>("ign", wildcard<int>("4'b00??")),
    illegal_bin<int>("ill", wildcard<int>("4'b10?1"))
  };
};

TEST(wildcard_bin, sampling) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    wildcard_cvg cvg("cvg", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(cvg.cvp.size(), 3u);

    cvg.sample(13);   // high
    cvg.sample(15);   // high
    cvg.sample(2);    // ignored
    cvg.sample(8);    // no bin
    cvg.sample(5);    // odd_low[0]
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(0), 2u);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cvg.cvp.get_bin_hit_count(2), 0u);
    EXPECT_EQ(cvg.cvp.get_misses(), 2u);
    EXPECT_DOUBLE_EQ(cvg.get_inst_coverage(), 200.0 / 3);

    EXPECT_THROW(cvg.sample(11), fc4sc::illegal_bin_sample_exception);
    cvg.sample(7);    // odd_low[1]
    EXPECT_EQ(cvg.get_inst_coverage(), 100);

    xml_printer::coverage_save("wildcard_bin_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  std::ifstream file("wildcard_bin_test.xml");
  std::stringstream content;
  content << file.rdbuf();
  const std::string xml = content.str();

  // a wildcard bin is written with the ranges of its values
  size_t pos = xml.find("<coverpointBin name=\"high\"");
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(xml.find("from=\"12\" \nto =\"15\"", pos), xml.find("<range", pos) + 8);
  EXPECT_NE(xml.find("<coverpointBin name=\"odd_low[0]\""), std::string::npos);
  EXPECT_NE(xml.find("<coverpointBin name=\"odd_low[1]\""), std::string::npos);
  pos = xml.find("<coverpointBin name=\"ill\"");
  ASSERT_NE(pos, std::string::npos);
  EXPECT_NE(xml.find("from=\"9\" \nto =\"9\"", pos), std::string::npos);
  EXPECT_NE(xml.find("from=\"11\" \nto =\"11\"", pos), std::string::npos);
}
