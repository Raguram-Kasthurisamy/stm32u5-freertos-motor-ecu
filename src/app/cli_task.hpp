// Author: Raguram Kasthurisamy

#pragma once

#include <span>

#include "cli/cli.hpp"
#include "drivers/uart_dma.hpp"
#include "io/text_writer.hpp"
#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "app/health.hpp"

namespace app {

// Owns the console: reads bytes from the UART and feeds them to the command line.
class CliTask final : public os::StaticTask<CliTask, os::task_config::kCliStackWords> {
 public:
  CliTask(drivers::UartDma& console, std::span<const cli::Command> commands);
  void set_health_id(HealthId id) { health_id_ = id; }

 private:
  friend class os::StaticTask<CliTask, os::task_config::kCliStackWords>;
  static constexpr std::uint32_t kReadTimeoutMs = 200;
  void run();

  drivers::UartDma& console_;
  io::TextWriter out_;  // declared before cli_, which keeps a reference to it
  cli::Cli cli_;
  HealthId health_id_ = kNoHealthId;
};

}  // namespace app
