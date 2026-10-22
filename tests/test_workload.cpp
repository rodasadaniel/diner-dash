#include "diner/workload.h"

#include <cmath>

#include <gtest/gtest.h>

namespace diner {
namespace {

TEST(WorkloadGeneratorTest, SameSeedProducesIdenticalWorkload) {
  WorkloadGenerator a(42);
  WorkloadGenerator b(42);
  const auto orders_a = a.Generate(500);
  const auto orders_b = b.Generate(500);
  ASSERT_EQ(orders_a.size(), orders_b.size());
  for (std::size_t i = 0; i < orders_a.size(); ++i) {
    EXPECT_EQ(orders_a[i].id, orders_b[i].id);
    EXPECT_DOUBLE_EQ(orders_a[i].arrival_time, orders_b[i].arrival_time);
    EXPECT_DOUBLE_EQ(orders_a[i].prep_time, orders_b[i].prep_time);
    EXPECT_DOUBLE_EQ(orders_a[i].value, orders_b[i].value);
  }
}

TEST(WorkloadGeneratorTest, DifferentSeedsProduceDifferentWorkloads) {
  WorkloadGenerator a(1);
  WorkloadGenerator b(2);
  const auto orders_a = a.Generate(100);
  const auto orders_b = b.Generate(100);
  bool any_difference = false;
  for (std::size_t i = 0; i < orders_a.size(); ++i) {
    if (orders_a[i].arrival_time != orders_b[i].arrival_time ||
        orders_a[i].prep_time != orders_b[i].prep_time) {
      any_difference = true;
    }
  }
  EXPECT_TRUE(any_difference);
}

TEST(WorkloadGeneratorTest, GeneratesRequestedCount) {
  WorkloadGenerator gen(9);
  EXPECT_EQ(gen.Generate(0).size(), 0u);
  EXPECT_EQ(gen.Generate(1).size(), 1u);
  EXPECT_EQ(gen.Generate(1234).size(), 1234u);
}

TEST(WorkloadGeneratorTest, ArrivalsAreNonDecreasing) {
  WorkloadGenerator gen(5);
  const auto orders = gen.Generate(5000);
  for (std::size_t i = 1; i < orders.size(); ++i) {
    EXPECT_GE(orders[i].arrival_time, orders[i - 1].arrival_time);
  }
}

TEST(WorkloadGeneratorTest, InterarrivalGapsArePositive) {
  WorkloadGenerator gen(5);
  const auto orders = gen.Generate(5000);
  EXPECT_GT(orders.front().arrival_time, 0.0);
  for (std::size_t i = 1; i < orders.size(); ++i) {
    EXPECT_GT(orders[i].arrival_time, orders[i - 1].arrival_time);
  }
}

TEST(WorkloadGeneratorTest, PrepTimesStayWithinMixtureBounds) {
  WorkloadGenerator gen(11);
  const auto orders = gen.Generate(5000);
  for (const Order& order : orders) {
    const bool in_short = order.prep_time >= WorkloadGenerator::kShortPrepMin &&
                          order.prep_time <= WorkloadGenerator::kShortPrepMax;
    const bool in_long = order.prep_time >= WorkloadGenerator::kLongPrepMin &&
                         order.prep_time <= WorkloadGenerator::kLongPrepMax;
    EXPECT_TRUE(in_short || in_long) << "prep=" << order.prep_time;
  }
}

TEST(WorkloadGeneratorTest, ValuesStayWithinBounds) {
  WorkloadGenerator gen(11);
  const auto orders = gen.Generate(5000);
  for (const Order& order : orders) {
    EXPECT_GE(order.value, WorkloadGenerator::kMinValue);
    EXPECT_LE(order.value, WorkloadGenerator::kMaxValue);
  }
}

TEST(WorkloadGeneratorTest, IdsAreSequentialFromZero) {
  WorkloadGenerator gen(3);
  const auto orders = gen.Generate(250);
  for (std::size_t i = 0; i < orders.size(); ++i) {
    EXPECT_EQ(orders[i].id, static_cast<std::uint64_t>(i));
  }
}

TEST(WorkloadGeneratorTest, LongOrderFractionIsApproximatelyThirtyPercent) {
  WorkloadGenerator gen(42);
  const auto orders = gen.Generate(20000);
  std::size_t long_count = 0;
  for (const Order& order : orders) {
    if (order.prep_time >= WorkloadGenerator::kLongPrepMin) ++long_count;
  }
  const double fraction =
      static_cast<double>(long_count) / static_cast<double>(orders.size());
  EXPECT_GT(fraction, 0.26);
  EXPECT_LT(fraction, 0.34);
}

TEST(WorkloadGeneratorTest, MeanInterarrivalIsApproximatelyConfigured) {
  WorkloadGenerator gen(7);
  const auto orders = gen.Generate(20000);
  const double mean_gap =
      orders.back().arrival_time / static_cast<double>(orders.size());
  EXPECT_GT(mean_gap, 90.0);
  EXPECT_LT(mean_gap, 108.0);
}

TEST(WorkloadGeneratorTest, LongOrdersCarryHigherValuesOnAverage) {
  WorkloadGenerator gen(21);
  const auto orders = gen.Generate(20000);
  double short_sum = 0.0;
  double long_sum = 0.0;
  std::size_t short_n = 0;
  std::size_t long_n = 0;
  for (const Order& order : orders) {
    if (order.prep_time >= WorkloadGenerator::kLongPrepMin) {
      long_sum += order.value;
      ++long_n;
    } else {
      short_sum += order.value;
      ++short_n;
    }
  }
  ASSERT_GT(long_n, 0u);
  ASSERT_GT(short_n, 0u);
  EXPECT_GT(long_sum / static_cast<double>(long_n),
            short_sum / static_cast<double>(short_n));
}

}  // namespace
}  // namespace diner
