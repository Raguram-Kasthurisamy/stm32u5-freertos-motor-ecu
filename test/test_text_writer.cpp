// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "io/text_writer.hpp"
#include "string_sink.hpp"

namespace {

template <typename Fn>
std::string format(Fn&& fn) {
  test::StringSink sink;
  io::TextWriter writer(sink);
  fn(writer);
  writer.flush();
  return sink.text;
}

}  // namespace

TEST(TextWriter, FormatsUnsignedDecimal) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(0); }), "0");
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(42); }), "42");
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(std::numeric_limits<std::uint32_t>::max()); }),
            "4294967295");
}

TEST(TextWriter, PadsUnsignedToWidth) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(42, 5); }), "   42");
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(7, 3, '0'); }), "007");
  EXPECT_EQ(format([](io::TextWriter& w) { w.u32(12345, 2); }), "12345");  // never truncates
}

TEST(TextWriter, FormatsSignedDecimal) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.i32(-123); }), "-123");
  EXPECT_EQ(format([](io::TextWriter& w) { w.i32(0); }), "0");
  EXPECT_EQ(format([](io::TextWriter& w) { w.i32(-5, 4); }), "  -5");
}

TEST(TextWriter, HandlesInt32MinWithoutOverflow) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.i32(std::numeric_limits<std::int32_t>::min()); }),
            "-2147483648");
}

TEST(TextWriter, FormatsHex) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.hex32(0xDEADBEEF); }), "DEADBEEF");
  EXPECT_EQ(format([](io::TextWriter& w) { w.hex32(0xAB, 4); }), "00AB");
  EXPECT_EQ(format([](io::TextWriter& w) { w.hex32(0x1234, 0); }), "00001234");  // 0 -> 8
}

TEST(TextWriter, LeftAlignsStrings) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.str("ab", 5).chr('|'); }), "ab   |");
  EXPECT_EQ(format([](io::TextWriter& w) { w.str("abcdef", 3); }), "abcdef");
}

TEST(TextWriter, BuffersUntilFlush) {
  test::StringSink sink;
  io::TextWriter writer(sink);
  writer.str("abc");
  EXPECT_EQ(sink.writes, 0);
  writer.flush();
  EXPECT_EQ(sink.writes, 1);
  EXPECT_EQ(sink.text, "abc");
  writer.flush();  // nothing pending: no empty write
  EXPECT_EQ(sink.writes, 1);
}

TEST(TextWriter, NewlineWritesCrLfAndFlushes) {
  test::StringSink sink;
  io::TextWriter writer(sink);
  writer.str("x").nl();
  EXPECT_EQ(sink.text, "x\r\n");
  EXPECT_EQ(sink.writes, 1);
}

TEST(TextWriter, SplitsOutputLargerThanItsBuffer) {
  const std::string long_text(300, 'x');
  test::StringSink sink;
  io::TextWriter writer(sink);
  writer.str(long_text);
  writer.flush();
  EXPECT_EQ(sink.text, long_text);
  EXPECT_EQ(sink.writes, 3);  // 128 + 128 + 44
}

TEST(TextWriter, FormatsFixedPoint) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(3.14159f, 2); }), "3.14");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(-2.5f, 1); }), "-2.5");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(0.0f, 3); }), "0.000");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(12.0f, 0); }), "12");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(0.125f, 2); }), "0.13");  // rounds half up
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(1.5f, 2, 8); }), "    1.50");
}

TEST(TextWriter, FixedPointHandlesEdgeCases) {
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(-0.001f, 2); }), "0.00");  // no "-0.00"
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(std::numeric_limits<float>::quiet_NaN(), 2); }), "nan");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(std::numeric_limits<float>::infinity(), 2); }), "inf");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(-std::numeric_limits<float>::infinity(), 2); }), "-inf");
  EXPECT_EQ(format([](io::TextWriter& w) { w.fixed(5.0e9f, 2); }), "ovf");
}
