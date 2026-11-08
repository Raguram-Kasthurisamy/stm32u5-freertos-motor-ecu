// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "cli/cli.hpp"
#include "string_sink.hpp"

namespace {

using Strings = std::vector<std::string>;

int g_calls = 0;
Strings g_last_args;

void record(io::TextWriter& out, cli::Args args) {
  ++g_calls;
  g_last_args.assign(args.begin(), args.end());
  out.str("ok").nl();
}

constexpr std::array kCommands{
    cli::Command{"echo", "print the arguments", &record},
    cli::Command{"led", "set the blink period", &record},
};

class CliTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_calls = 0;
    g_last_args.clear();
  }

  // Feeds characters the way the CLI task does, then flushes like the task does.
  void type(std::string_view input) {
    for (const char c : input) {
      cli.feed(c);
    }
    out.flush();
  }

  bool output_contains(std::string_view needle) const {
    return sink.text.find(needle) != std::string::npos;
  }

  test::StringSink sink;
  io::TextWriter out{sink};
  cli::Cli cli{kCommands, out};
};

}  // namespace

TEST_F(CliTest, DispatchesCommandWithArguments) {
  type("echo a  bb\tccc\r");
  EXPECT_EQ(g_calls, 1);
  EXPECT_EQ(g_last_args, (Strings{"echo", "a", "bb", "ccc"}));
}

TEST_F(CliTest, AcceptsCrLfAndCrlfEndingsWithoutRunningTwice) {
  type("echo 1\r");
  type("echo 2\n");
  type("echo 3\r\n");
  EXPECT_EQ(g_calls, 3);
}

TEST_F(CliTest, EchoesTypedCharacters) {
  type("ec");
  EXPECT_EQ(sink.text, "ec");
}

TEST_F(CliTest, BackspaceAndDeleteEditTheLine) {
  type("echx\bo hi!\x7f\r");
  EXPECT_EQ(g_last_args, (Strings{"echo", "hi"}));
  EXPECT_TRUE(output_contains("\b \b"));
}

TEST_F(CliTest, BackspaceOnEmptyLineIsIgnored) {
  type("\b\x7f");
  EXPECT_EQ(sink.text, "");
}

TEST_F(CliTest, IgnoresArrowKeyEscapeSequences) {
  type("ec\x1b[Aho\x1b[1;5C x\r");
  EXPECT_EQ(g_last_args, (Strings{"echo", "x"}));
}

TEST_F(CliTest, CtrlCDiscardsTheLine) {
  type("echo x\x03");
  type("\r");
  EXPECT_EQ(g_calls, 0);
  EXPECT_TRUE(output_contains("^C"));
}

TEST_F(CliTest, ReportsUnknownCommand) {
  type("nope\r");
  EXPECT_EQ(g_calls, 0);
  EXPECT_TRUE(output_contains("unknown command: nope"));
}

TEST_F(CliTest, RejectsTooManyArguments) {
  type("echo 1 2 3 4 5 6 7 8\r");  // 9 tokens, limit is 8
  EXPECT_EQ(g_calls, 0);
  EXPECT_TRUE(output_contains("too many arguments"));
}

TEST_F(CliTest, AcceptsExactlyMaxArguments) {
  type("echo 1 2 3 4 5 6 7\r");  // 8 tokens
  EXPECT_EQ(g_calls, 1);
  EXPECT_EQ(g_last_args.size(), cli::Cli::kMaxArgs);
}

TEST_F(CliTest, EmptyLineJustPromptsAgain) {
  type("   \r");
  EXPECT_EQ(g_calls, 0);
  EXPECT_EQ(sink.text, "   \r\necu> ");
}

TEST_F(CliTest, RefusesCharactersBeyondLineLengthWithBell) {
  const std::string input(cli::Cli::kMaxLineLength + 5, 'a');
  type(input);
  std::size_t bells = 0;
  for (const char c : sink.text) {
    bells += (c == '\a') ? 1U : 0U;
  }
  EXPECT_EQ(bells, 5U);
}

TEST_F(CliTest, HelpListsEveryCommand) {
  type("help\r");
  EXPECT_TRUE(output_contains("help"));
  EXPECT_TRUE(output_contains("echo"));
  EXPECT_TRUE(output_contains("print the arguments"));
  EXPECT_TRUE(output_contains("led"));
  EXPECT_EQ(g_calls, 0);
}

TEST(Tokenize, SplitsOnSpacesAndTabs) {
  std::array<std::string_view, 4> tokens{};
  const auto count = cli::tokenize("  a\tbc  d ", tokens);
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(*count, 3U);
  EXPECT_EQ(tokens[0], "a");
  EXPECT_EQ(tokens[1], "bc");
  EXPECT_EQ(tokens[2], "d");
}

TEST(Tokenize, EmptyAndBlankLinesHaveNoTokens) {
  std::array<std::string_view, 4> tokens{};
  EXPECT_EQ(cli::tokenize("", tokens), 0U);
  EXPECT_EQ(cli::tokenize(" \t ", tokens), 0U);
}

TEST(Tokenize, ReportsOverflow) {
  std::array<std::string_view, 2> tokens{};
  EXPECT_FALSE(cli::tokenize("a b c", tokens).has_value());
}
