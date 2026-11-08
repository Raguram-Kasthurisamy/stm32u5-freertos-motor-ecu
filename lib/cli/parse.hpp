// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace cli {

// Parses an unsigned decimal ("42") or 0x-prefixed hex ("0x2A") number.
// Rejects empty input, signs, whitespace, trailing characters and values above UINT32_MAX.
[[nodiscard]] std::optional<std::uint32_t> parse_u32(std::string_view text);

// Parses a plain decimal number: optional sign, up to 9 integer and 6 fractional digits
// ("-12.5", "+3", ".25", "7."). No exponents, no inf/nan. Written by hand because
// libstdc++'s floating-point std::from_chars may allocate memory on some inputs.
[[nodiscard]] std::optional<float> parse_float(std::string_view text);

}  // namespace cli
