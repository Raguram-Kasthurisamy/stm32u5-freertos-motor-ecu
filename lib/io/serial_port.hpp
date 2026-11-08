// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace io {

inline constexpr std::uint32_t kWaitForever = 0xFFFF'FFFFU;

// Anything that accepts a stream of bytes: a UART, a USB CDC port, or a test double.
// The destructor is protected and non-virtual on purpose: objects are never deleted
// through this interface (there is no heap), so no vtable slot or operator delete is needed.
class ByteSink {
 public:
  // Blocks until every byte has been handed to the hardware (or an error occurred).
  virtual void write(std::span<const std::uint8_t> data) = 0;

 protected:
  ByteSink() = default;
  ~ByteSink() = default;
  ByteSink(const ByteSink&) = default;
  ByteSink& operator=(const ByteSink&) = default;
};

class SerialPort : public ByteSink {
 public:
  // Blocks until at least one byte is available or the timeout expires.
  // Returns the number of bytes copied into `buffer` (0 on timeout).
  virtual std::size_t read(std::span<std::uint8_t> buffer, std::uint32_t timeout_ms) = 0;

 protected:
  SerialPort() = default;
  ~SerialPort() = default;
  SerialPort(const SerialPort&) = default;
  SerialPort& operator=(const SerialPort&) = default;
};

}  // namespace io
