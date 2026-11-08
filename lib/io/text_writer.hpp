// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "io/serial_port.hpp"

namespace io {

// Small, allocation-free text formatter (a printf replacement).
//
// Output is collected in an internal buffer and handed to the sink in chunks, so one
// line of output costs one DMA transfer instead of one per character. Nothing here
// touches the heap, uses varargs, or pulls in newlib's printf machinery.
class TextWriter {
 public:
  static constexpr std::size_t kBufferSize = 128;

  explicit TextWriter(ByteSink& sink) : sink_(sink) {}

  TextWriter& str(std::string_view text);
  // Left-aligned text, padded with spaces up to `width` characters.
  TextWriter& str(std::string_view text, std::size_t width);
  TextWriter& chr(char c);
  // Unsigned decimal, right-aligned in `width` characters using `pad`.
  TextWriter& u32(std::uint32_t value, std::size_t width = 0, char pad = ' ');
  // Signed decimal, right-aligned in `width` characters.
  TextWriter& i32(std::int32_t value, std::size_t width = 0);
  // Upper-case hex with exactly `digits` digits (1..8), no "0x" prefix.
  TextWriter& hex32(std::uint32_t value, std::size_t digits = 8);
  // Fixed-point decimal ("-12.35"), right-aligned in `width` characters. decimals <= 6.
  // Prints "nan", "inf", "-inf", or "ovf" for magnitudes of 4e9 and above.
  TextWriter& fixed(float value, std::size_t decimals, std::size_t width = 0);
  // Writes "\r\n" and flushes.
  TextWriter& nl();

  void flush();

 private:
  void put(char c);
  void repeat(char c, std::size_t count);

  ByteSink& sink_;
  std::array<std::uint8_t, kBufferSize> buffer_{};
  std::size_t used_ = 0;
};

}  // namespace io
