// Author: Raguram Kasthurisamy

#pragma once

#include "cli/cli.hpp"

// Console commands for the motor controller (registered in cli_commands.cpp).
namespace app::motor_commands {

void motor(io::TextWriter& out, cli::Args args);
void duty(io::TextWriter& out, cli::Args args);
void speed(io::TextWriter& out, cli::Args args);
void stop(io::TextWriter& out, cli::Args args);
void pid(io::TextWriter& out, cli::Args args);
void sim(io::TextWriter& out, cli::Args args);
void clear(io::TextWriter& out, cli::Args args);
void ctrl(io::TextWriter& out, cli::Args args);
void stream(io::TextWriter& out, cli::Args args);

}  // namespace app::motor_commands
