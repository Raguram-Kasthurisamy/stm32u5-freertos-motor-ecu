// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include "safety/task_monitor.hpp"

TEST(TaskMonitor, NoStallWhileTasksCheckIn) {
  safety::TaskMonitor monitor;
  const auto a = monitor.add("control", 50, 0);
  const auto b = monitor.add("cli", 1000, 0);
  ASSERT_TRUE(a && b);
  for (std::uint32_t t = 0; t <= 2000; t += 10) {
    monitor.checkin(*a, t);
    if (t % 500 == 0) {
      monitor.checkin(*b, t);
    }
    EXPECT_FALSE(monitor.find_stalled(t).has_value());
  }
}

TEST(TaskMonitor, ReportsTaskThatMissesItsDeadline) {
  safety::TaskMonitor monitor;
  const auto control = monitor.add("control", 50, 0);
  const auto cli = monitor.add("cli", 1000, 0);
  monitor.checkin(*cli, 40);
  EXPECT_FALSE(monitor.find_stalled(50).has_value());
  const auto stalled = monitor.find_stalled(51);
  ASSERT_TRUE(stalled.has_value());
  EXPECT_EQ(*stalled, *control);
  EXPECT_STREQ(monitor.name(*stalled), "control");
}

TEST(TaskMonitor, AgeIsCorrectAcrossMillisecondWrap) {
  safety::TaskMonitor monitor;
  const auto id = monitor.add("task", 100, 0xFFFF'FFF0U);
  EXPECT_EQ(monitor.age_ms(*id, 0x0000'0010U), 0x20U);
  EXPECT_FALSE(monitor.find_stalled(0x0000'0010U).has_value());
}

TEST(TaskMonitor, RefusesMoreThanCapacity) {
  safety::TaskMonitor monitor;
  for (std::size_t i = 0; i < safety::TaskMonitor::kMaxTasks; ++i) {
    EXPECT_TRUE(monitor.add("t", 10, 0).has_value());
  }
  EXPECT_FALSE(monitor.add("one too many", 10, 0).has_value());
}
