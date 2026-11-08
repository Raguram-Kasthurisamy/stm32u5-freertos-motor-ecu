// Author: Raguram Kasthurisamy

#pragma once

#include <string>

#include "io/serial_port.hpp"

namespace test {

// Records everything written to it, and how many write() calls it took.
class StringSink final : public io::ByteSink {
 public:
  void write(std::span<const std::uint8_t> data) override {
    ++writes;
    text.append(reinterpret_cast<const char*>(data.data()), data.size());
  }

  std::string text;
  int writes = 0;
};

}  // namespace test
