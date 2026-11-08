// Author: Raguram Kasthurisamy

#include "io/text_writer.hpp"

#include <cmath>

namespace io {
namespace {

constexpr std::size_t kMaxDecimalDigits = 10;  // 4294967295

// Writes the decimal digits of `value` into `out`, least significant first.
// Returns the number of digits written.
std::size_t to_reversed_digits(std::uint32_t value, std::array<char, kMaxDecimalDigits>& out) {
  std::size_t count = 0;
  do {
    out[count++] = static_cast<char>('0' + value % 10U);
    value /= 10U;
  } while (value != 0U);
  return count;
}

}  // namespace

void TextWriter::put(char c) {
  if (used_ == buffer_.size()) {
    flush();
  }
  buffer_[used_++] = static_cast<std::uint8_t>(c);
}

void TextWriter::repeat(char c, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    put(c);
  }
}

void TextWriter::flush() {
  if (used_ == 0) {
    return;
  }
  sink_.write(std::span<const std::uint8_t>(buffer_.data(), used_));
  used_ = 0;
}

TextWriter& TextWriter::str(std::string_view text) {
  for (const char c : text) {
    put(c);
  }
  return *this;
}

TextWriter& TextWriter::str(std::string_view text, std::size_t width) {
  str(text);
  if (text.size() < width) {
    repeat(' ', width - text.size());
  }
  return *this;
}

TextWriter& TextWriter::chr(char c) {
  put(c);
  return *this;
}

TextWriter& TextWriter::u32(std::uint32_t value, std::size_t width, char pad) {
  std::array<char, kMaxDecimalDigits> digits{};
  std::size_t count = to_reversed_digits(value, digits);
  if (count < width) {
    repeat(pad, width - count);
  }
  while (count > 0) {
    put(digits[--count]);
  }
  return *this;
}

TextWriter& TextWriter::i32(std::int32_t value, std::size_t width) {
  const bool negative = value < 0;
  // Negate in unsigned arithmetic so INT32_MIN does not overflow.
  const std::uint32_t magnitude =
      negative ? 0U - static_cast<std::uint32_t>(value) : static_cast<std::uint32_t>(value);

  std::array<char, kMaxDecimalDigits> digits{};
  std::size_t count = to_reversed_digits(magnitude, digits);
  const std::size_t total = count + (negative ? 1U : 0U);
  if (total < width) {
    repeat(' ', width - total);
  }
  if (negative) {
    put('-');
  }
  while (count > 0) {
    put(digits[--count]);
  }
  return *this;
}

TextWriter& TextWriter::hex32(std::uint32_t value, std::size_t digits) {
  constexpr std::string_view kHexDigits = "0123456789ABCDEF";
  if (digits == 0 || digits > 8) {
    digits = 8;
  }
  for (std::size_t i = digits; i > 0; --i) {
    const std::uint32_t nibble = (value >> ((i - 1U) * 4U)) & 0xFU;
    put(kHexDigits[nibble]);
  }
  return *this;
}

TextWriter& TextWriter::fixed(float value, std::size_t decimals, std::size_t width) {
  // Built in a local buffer first so the total width is known before padding.
  std::array<char, 24> text{};
  std::size_t length = 0;
  const auto append = [&](std::string_view part) {
    for (const char c : part) {
      if (length < text.size()) {
        text[length++] = c;
      }
    }
  };
  const auto append_digits = [&](std::uint32_t number, std::size_t min_digits) {
    std::array<char, kMaxDecimalDigits> digits{};
    std::size_t count = to_reversed_digits(number, digits);
    for (std::size_t i = count; i < min_digits; ++i) {
      append("0");
    }
    while (count > 0) {
      const char c = digits[--count];
      append(std::string_view(&c, 1));
    }
  };

  if (std::isnan(value)) {
    append("nan");
  } else if (std::isinf(value)) {
    append(value < 0.0f ? "-inf" : "inf");
  } else {
    decimals = decimals > 6 ? 6 : decimals;
    std::uint32_t scale = 1;
    for (std::size_t i = 0; i < decimals; ++i) {
      scale *= 10U;
    }
    const bool negative = value < 0.0f;
    const float magnitude = negative ? -value : value;
    if (magnitude >= 4.0e9f) {
      append("ovf");
    } else {
      const auto scaled =
          static_cast<std::uint64_t>(magnitude * static_cast<float>(scale) + 0.5f);
      if (negative && scaled != 0U) {
        append("-");  // no "-0.00" for tiny negative values
      }
      append_digits(static_cast<std::uint32_t>(scaled / scale), 1);
      if (decimals > 0) {
        append(".");
        append_digits(static_cast<std::uint32_t>(scaled % scale), decimals);
      }
    }
  }

  if (length < width) {
    repeat(' ', width - length);
  }
  for (std::size_t i = 0; i < length; ++i) {
    put(text[i]);
  }
  return *this;
}

TextWriter& TextWriter::nl() {
  str("\r\n");
  flush();
  return *this;
}

}  // namespace io
