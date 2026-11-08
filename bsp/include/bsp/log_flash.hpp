// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// The flash page reserved for the persistent fault log (see the linker script).
namespace bsp::log_flash {

inline constexpr std::size_t kWriteUnit = 16;  // the U5 programs flash 128 bits at a time

// Memory-mapped view of the region.
[[nodiscard]] std::span<const std::uint8_t> region();
bool erase();
// `offset` and `data.size()` must be multiples of kWriteUnit, inside erased flash.
bool program(std::size_t offset, std::span<const std::uint8_t> data);

}  // namespace bsp::log_flash
