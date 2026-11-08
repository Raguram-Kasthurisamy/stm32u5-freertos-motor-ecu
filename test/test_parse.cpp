// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include "cli/parse.hpp"

TEST(ParseU32, AcceptsDecimal) {
  EXPECT_EQ(cli::parse_u32("0"), 0U);
  EXPECT_EQ(cli::parse_u32("42"), 42U);
  EXPECT_EQ(cli::parse_u32("4294967295"), 4294967295U);
}

TEST(ParseU32, AcceptsHexWithPrefix) {
  EXPECT_EQ(cli::parse_u32("0x1F"), 0x1FU);
  EXPECT_EQ(cli::parse_u32("0XfF"), 0xFFU);
  EXPECT_EQ(cli::parse_u32("0xFFFFFFFF"), 0xFFFFFFFFU);
}

TEST(ParseU32, RejectsOverflow) {
  EXPECT_FALSE(cli::parse_u32("4294967296").has_value());
  EXPECT_FALSE(cli::parse_u32("0x100000000").has_value());
}

TEST(ParseU32, RejectsMalformedInput) {
  EXPECT_FALSE(cli::parse_u32("").has_value());
  EXPECT_FALSE(cli::parse_u32("0x").has_value());
  EXPECT_FALSE(cli::parse_u32("12a").has_value());
  EXPECT_FALSE(cli::parse_u32("-1").has_value());
  EXPECT_FALSE(cli::parse_u32("+5").has_value());
  EXPECT_FALSE(cli::parse_u32(" 5").has_value());
  EXPECT_FALSE(cli::parse_u32("5 ").has_value());
}

TEST(ParseFloat, AcceptsPlainDecimals) {
  EXPECT_FLOAT_EQ(*cli::parse_float("0"), 0.0f);
  EXPECT_FLOAT_EQ(*cli::parse_float("12.5"), 12.5f);
  EXPECT_FLOAT_EQ(*cli::parse_float("-3.25"), -3.25f);
  EXPECT_FLOAT_EQ(*cli::parse_float("+7"), 7.0f);
  EXPECT_FLOAT_EQ(*cli::parse_float(".5"), 0.5f);
  EXPECT_FLOAT_EQ(*cli::parse_float("2."), 2.0f);
  EXPECT_NEAR(*cli::parse_float("0.003"), 0.003f, 1e-9f);
}

TEST(ParseFloat, RejectsMalformedInput) {
  EXPECT_FALSE(cli::parse_float("").has_value());
  EXPECT_FALSE(cli::parse_float("-").has_value());
  EXPECT_FALSE(cli::parse_float(".").has_value());
  EXPECT_FALSE(cli::parse_float("1.2.3").has_value());
  EXPECT_FALSE(cli::parse_float("1e3").has_value());
  EXPECT_FALSE(cli::parse_float("abc").has_value());
  EXPECT_FALSE(cli::parse_float("1234567890").has_value());  // > 9 integer digits
  EXPECT_FALSE(cli::parse_float("0.1234567").has_value());   // > 6 fraction digits
}
