// Author: Raguram Kasthurisamy

#pragma once

#include "cli/cli.hpp"
#include "safety/crash_record.hpp"

// Console commands for CAN, health monitoring, the fault log and fault injection.
namespace app::system_commands {

void can(io::TextWriter& out, cli::Args args);
void cantx(io::TextWriter& out, cli::Args args);
void health(io::TextWriter& out, cli::Args args);
void faults(io::TextWriter& out, cli::Args args);
void fault(io::TextWriter& out, cli::Args args);
void print_fault_record(io::TextWriter& out, const safety::CrashRecord& record);

}  // namespace app::system_commands
