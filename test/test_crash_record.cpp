// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <vector>
#include <cstring>

#include "safety/crash_record.hpp"
#include "safety/fault_log_format.hpp"

namespace {

safety::CrashRecord make_record(std::uint32_t sequence) {
  safety::CrashRecord record{};
  record.sequence = sequence;
  record.type = safety::FaultType::kUsageFault;
  record.pc = 0x0800'1234U;
  safety::set_text(record, "control");
  safety::seal(record);
  return record;
}

}  // namespace

TEST(CrashRecord, SealedRecordIsValid) {
  const safety::CrashRecord record = make_record(1);
  EXPECT_TRUE(safety::is_valid(record));
  EXPECT_EQ(safety::text(record), "control");
}

TEST(CrashRecord, CorruptionIsDetected) {
  safety::CrashRecord record = make_record(1);
  record.pc ^= 0x10U;
  EXPECT_FALSE(safety::is_valid(record));
}

TEST(CrashRecord, ZeroedOrRandomRamIsNotValid) {
  safety::CrashRecord zeroed{};
  EXPECT_FALSE(safety::is_valid(zeroed));
}

TEST(CrashRecord, LongTextKeepsItsEnd) {
  safety::CrashRecord record{};
  safety::set_text(record, "src/drivers/very_long_file_name.cpp");
  EXPECT_EQ(safety::text(record).size(), 19U);
  EXPECT_TRUE(safety::text(record).ends_with("file_name.cpp"));
}

TEST(FaultLogFormat, EmptyErasedRegion) {
  const std::vector<std::uint8_t> flash(1024, 0xFF);
  EXPECT_EQ(safety::fault_log::capacity(flash), 16U);
  EXPECT_EQ(safety::fault_log::used_slots(flash), 0U);
  EXPECT_EQ(safety::fault_log::next_sequence(flash), 1U);
  EXPECT_FALSE(safety::fault_log::read_slot(flash, 0).has_value());
}

TEST(FaultLogFormat, ReadsRecordsAndContinuesSequence) {
  std::vector<std::uint8_t> flash(1024, 0xFF);
  const safety::CrashRecord first = make_record(7);
  const safety::CrashRecord second = make_record(8);
  std::memcpy(flash.data(), &first, sizeof(first));
  std::memcpy(flash.data() + 64, &second, sizeof(second));
  EXPECT_EQ(safety::fault_log::used_slots(flash), 2U);
  EXPECT_EQ(safety::fault_log::next_sequence(flash), 9U);
  ASSERT_TRUE(safety::fault_log::read_slot(flash, 1).has_value());
  EXPECT_EQ(safety::fault_log::read_slot(flash, 1)->sequence, 8U);
}

TEST(FaultLogFormat, CorruptedSlotCountsAsUsedButIsNotReturned) {
  std::vector<std::uint8_t> flash(1024, 0xFF);
  safety::CrashRecord broken = make_record(3);
  broken.lr = 0xDEAD;  // changed after sealing
  std::memcpy(flash.data(), &broken, sizeof(broken));
  EXPECT_EQ(safety::fault_log::used_slots(flash), 1U);
  EXPECT_FALSE(safety::fault_log::read_slot(flash, 0).has_value());
  EXPECT_EQ(safety::fault_log::next_sequence(flash), 1U);
}

TEST(CrashRecord, DescribesFaultStatusBits) {
  EXPECT_STREQ(safety::describe_cfsr(safety::kCfsrStackOverflow), "stack limit hit");
  EXPECT_STREQ(safety::describe_cfsr(1U << 25), "divide by zero");
  EXPECT_STREQ(safety::describe_cfsr((1U << 9) | safety::kCfsrBfarValid), "precise bus error");
  EXPECT_STREQ(safety::describe_cfsr(0), "no fault bits");
}
