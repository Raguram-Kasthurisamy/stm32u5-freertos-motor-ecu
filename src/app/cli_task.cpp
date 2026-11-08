// Author: Raguram Kasthurisamy

#include "app/cli_task.hpp"

#include <array>
#include <cstdint>

#include "app/system_commands.hpp"
#include "bsp/board.hpp"
#include "system/boot_report.hpp"

namespace app {

CliTask::CliTask(drivers::UartDma& console, std::span<const cli::Command> commands)
    : console_(console), out_(console), cli_(commands, out_) {}

void CliTask::run() {
  console_.start_rx();

  out_.nl().str("ecu-firmware ").str(ECU_FW_VERSION).str(" on ").str(bsp::board_name()).nl();
  out_.str("reset cause: ").str(bsp::to_string(bsp::reset_cause())).str("  |  type 'help'").nl();
  if (const auto& last = sys::boot::last_fault()) {
    out_.str("the previous run ended with a fault:").nl();
    system_commands::print_fault_record(out_, *last);
  }
  cli_.prompt();

  std::array<std::uint8_t, 32> buffer{};
  for (;;) {
    // Wake at least every 200 ms even without input, to check in with the supervisor.
    const std::size_t count = console_.read(buffer, kReadTimeoutMs);
    health_checkin(health_id_);
    for (std::size_t i = 0; i < count; ++i) {
      cli_.feed(static_cast<char>(buffer[i]));
    }
    // One DMA transfer echoes a whole burst (e.g. pasted text) instead of one per byte.
    out_.flush();
  }
}

}  // namespace app
