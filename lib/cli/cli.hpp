// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "io/text_writer.hpp"

namespace cli {

// args[0] is the command name. The views point into the CLI's line buffer and are only
// valid for the duration of the handler call.
using Args = std::span<const std::string_view>;
using Handler = void (*)(io::TextWriter& out, Args args);

struct Command {
  std::string_view name;
  std::string_view help;
  Handler handler;
};

// Interactive command line: line editing, echo, tokenizing and dispatch.
//
// Hardware independent and allocation free: it consumes characters through feed() and
// produces output through a TextWriter, so the whole thing is unit tested on the host.
// Echoed characters are buffered; the owner flushes the writer after each batch of input.
class Cli {
 public:
  static constexpr std::size_t kMaxLineLength = 80;
  static constexpr std::size_t kMaxArgs = 8;

  Cli(std::span<const Command> commands, io::TextWriter& out, std::string_view prompt = "ecu> ");

  void feed(char c);
  void prompt();

 private:
  enum class EscapeState : std::uint8_t { kNone, kEscape, kCsi };

  void end_of_line();
  void execute();
  void print_help();
  void clear_line();

  std::span<const Command> commands_;
  io::TextWriter& out_;
  std::string_view prompt_;
  std::array<char, kMaxLineLength> line_{};
  std::size_t length_ = 0;
  bool last_was_cr_ = false;
  EscapeState escape_ = EscapeState::kNone;
};

// Splits `line` on spaces and tabs into `tokens`.
// Returns the number of tokens, or std::nullopt if there are more than tokens.size().
[[nodiscard]] std::optional<std::size_t> tokenize(std::string_view line,
                                                  std::span<std::string_view> tokens);

}  // namespace cli
