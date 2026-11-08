// Author: Raguram Kasthurisamy

#include "cli/parse.hpp"

#include <charconv>
#include <system_error>

namespace cli {
namespace {

// Accumulates a run of decimal digits (an empty run is 0). False on any non-digit.
bool digits_to_u32(std::string_view digits, std::uint32_t& value) {
  value = 0;
  for (const char c : digits) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10U + static_cast<std::uint32_t>(c - '0');
  }
  return true;
}

}  // namespace

std::optional<std::uint32_t> parse_u32(std::string_view text) {
  int base = 10;
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    base = 16;
    text.remove_prefix(2);
  }
  if (text.empty()) {
    return std::nullopt;
  }

  // std::from_chars is locale independent, never allocates, and reports overflow;
  // it also rejects a leading '+', '-' or whitespace for unsigned types.
  std::uint32_t value = 0;
  const char* const first = text.data();
  const char* const last = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(first, last, value, base);
  if (error != std::errc{} || ptr != last) {
    return std::nullopt;
  }
  return value;
}

std::optional<float> parse_float(std::string_view text) {
  constexpr std::size_t kMaxIntegerDigits = 9;
  constexpr std::size_t kMaxFractionDigits = 6;

  bool negative = false;
  if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
    negative = text[0] == '-';
    text.remove_prefix(1);
  }
  const std::size_t dot = text.find('.');
  const std::string_view integer_digits(text.data(), dot == std::string_view::npos ? text.size() : dot);
  const std::string_view fraction_digits =
      dot == std::string_view::npos ? std::string_view{}
                                    : std::string_view(text.data() + dot + 1, text.size() - dot - 1);

  if ((integer_digits.empty() && fraction_digits.empty()) ||
      integer_digits.size() > kMaxIntegerDigits || fraction_digits.size() > kMaxFractionDigits) {
    return std::nullopt;
  }
  std::uint32_t integer = 0;
  std::uint32_t fraction = 0;
  if (!digits_to_u32(integer_digits, integer) || !digits_to_u32(fraction_digits, fraction)) {
    return std::nullopt;
  }
  std::uint32_t scale = 1;
  for (std::size_t i = 0; i < fraction_digits.size(); ++i) {
    scale *= 10U;
  }
  const float value = static_cast<float>(integer) + static_cast<float>(fraction) / static_cast<float>(scale);
  return negative ? -value : value;
}

}  // namespace cli
