// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

// Board support interface. Every folder under bsp/ implements this for one board, so
// application code never names a specific MCU, pin or clock tree.
namespace bsp {

enum class ClockSource : std::uint8_t {
  kCrystalReferenced,  // derived from or trimmed against a crystal: accurate enough for CAN
  kInternalFallback,   // internal RC oscillator only: the crystal did not start
};

enum class ResetCause : std::uint8_t {
  kPowerOnOrBrownout,
  kPin,
  kSoftware,
  kIndependentWatchdog,
  kWindowWatchdog,
  kLowPower,
  kOptionByteLoad,
  kUnknown,
};

// Brings up power, clocks, GPIO and the console UART. Must run before any FreeRTOS
// object is created (see the note in main.cpp).
void init();

[[nodiscard]] const char* board_name();
[[nodiscard]] ClockSource clock_source();
[[nodiscard]] const char* clock_description();  // board-specific detail of the clock tree
[[nodiscard]] ResetCause reset_cause();
[[nodiscard]] std::uint32_t core_clock_hz();

void led_set(bool on);
void led_toggle();

[[noreturn]] void system_reset();

// Implemented by the application (src/system/fatal.cpp). The BSP calls it when hardware
// init fails or the CPU faults. Keeps the dependency pointing upward through one function.
[[noreturn]] void fatal_error_hook(const char* reason, const char* where, int line);

[[nodiscard]] constexpr const char* to_string(ResetCause cause) {
  switch (cause) {
    case ResetCause::kPowerOnOrBrownout: return "power-on / brownout";
    case ResetCause::kPin: return "reset pin";
    case ResetCause::kSoftware: return "software";
    case ResetCause::kIndependentWatchdog: return "independent watchdog";
    case ResetCause::kWindowWatchdog: return "window watchdog";
    case ResetCause::kLowPower: return "low-power";
    case ResetCause::kOptionByteLoad: return "option byte load";
    case ResetCause::kUnknown: break;
  }
  return "unknown";
}

}  // namespace bsp
