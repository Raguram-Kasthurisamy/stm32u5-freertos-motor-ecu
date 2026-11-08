// Author: Raguram Kasthurisamy

#include "cli/cli.hpp"

#include <algorithm>

namespace cli {
namespace {

constexpr std::string_view kWhitespace = " \t";
constexpr char kCtrlC = '\x03';
constexpr char kBackspace = '\b';
constexpr char kDelete = '\x7f';
constexpr char kEscape = '\x1b';
constexpr char kBell = '\a';

}  // namespace

std::optional<std::size_t> tokenize(std::string_view line, std::span<std::string_view> tokens) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while (true) {
    pos = line.find_first_not_of(kWhitespace, pos);
    if (pos == std::string_view::npos) {
      break;
    }
    std::size_t end = line.find_first_of(kWhitespace, pos);
    if (end == std::string_view::npos) {
      end = line.size();
    }
    if (count == tokens.size()) {
      return std::nullopt;
    }
    // Not line.substr(): substr can throw std::out_of_range, which drags libstdc++'s
    // exception machinery (and malloc) into a -fno-exceptions firmware image.
    tokens[count++] = std::string_view(line.data() + pos, end - pos);
    pos = end;
  }
  return count;
}

Cli::Cli(std::span<const Command> commands, io::TextWriter& out, std::string_view prompt)
    : commands_(commands), out_(out), prompt_(prompt) {}

void Cli::prompt() {
  out_.str(prompt_);
  out_.flush();
}

void Cli::clear_line() { length_ = 0; }

void Cli::feed(char c) {
  // Swallow ANSI escape sequences (arrow keys, Home/End...): ESC '[' params final-byte.
  switch (escape_) {
    case EscapeState::kEscape:
      escape_ = (c == '[') ? EscapeState::kCsi : EscapeState::kNone;
      return;
    case EscapeState::kCsi:
      if (c >= 0x40 && c <= 0x7E) {
        escape_ = EscapeState::kNone;
      }
      return;
    case EscapeState::kNone:
      break;
  }

  const bool previous_was_cr = last_was_cr_;
  last_was_cr_ = (c == '\r');

  if (c == '\t') {
    c = ' ';  // no tab completion (yet): treat pasted tabs as separators
  }

  switch (c) {
    case '\r':
      end_of_line();
      return;
    case '\n':
      // Accept CR, LF and CRLF line endings without running a command twice.
      if (!previous_was_cr) {
        end_of_line();
      }
      return;
    case kEscape:
      escape_ = EscapeState::kEscape;
      return;
    case kCtrlC:
      out_.str("^C").nl();
      clear_line();
      prompt();
      return;
    case kBackspace:
    case kDelete:
      if (length_ > 0) {
        --length_;
        out_.str("\b \b");
      }
      return;
    default:
      break;
  }

  if (c < 0x20 || c > 0x7E) {
    return;  // other control characters and non-ASCII bytes
  }
  if (length_ < line_.size()) {
    line_[length_++] = c;
    out_.chr(c);
  } else {
    out_.chr(kBell);  // line full: refuse the character audibly
  }
}

void Cli::end_of_line() {
  out_.nl();
  execute();
  clear_line();
  prompt();
}

void Cli::execute() {
  const std::string_view line(line_.data(), length_);
  std::array<std::string_view, kMaxArgs> tokens{};
  const std::optional<std::size_t> count = tokenize(line, tokens);

  if (!count) {
    out_.str("error: too many arguments (max ")
        .u32(static_cast<std::uint32_t>(kMaxArgs - 1))
        .chr(')')
        .nl();
    return;
  }
  if (*count == 0) {
    return;  // empty line: just show the prompt again
  }

  const Args args(tokens.data(), *count);
  if (args[0] == "help") {
    print_help();
    return;
  }
  for (const Command& command : commands_) {
    if (command.name == args[0]) {
      command.handler(out_, args);
      return;
    }
  }
  out_.str("unknown command: ").str(args[0]).str(" (try 'help')").nl();
}

void Cli::print_help() {
  constexpr std::string_view kHelpName = "help";
  std::size_t width = kHelpName.size();
  for (const Command& command : commands_) {
    width = std::max(width, command.name.size());
  }
  width += 2;

  out_.str("  ").str(kHelpName, width).str("list commands").nl();
  for (const Command& command : commands_) {
    out_.str("  ").str(command.name, width).str(command.help).nl();
  }
}

}  // namespace cli
