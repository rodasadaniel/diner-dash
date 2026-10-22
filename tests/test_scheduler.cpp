#include "diner/scheduler.h"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

namespace diner {
namespace {

Order MakeOrder(std::uint64_t id, double arrival, double prep, double value) {
  Order order;
  order.id = id;
  order.arrival_time = arrival;
  order.prep_time = prep;
  order.value = value;
  return order;
}

// -------------------------------- FIFO -------------------------------------

TEST(FifoSchedulerTest, StartsEmpty) {
  FifoScheduler scheduler;
  EXPECT_TRUE(scheduler.Empty());
  EXPECT_EQ(scheduler.Size(), 0u);
}

TEST(FifoSchedulerTest, PopOnEmptyReturnsNullopt) {
  FifoScheduler scheduler;
  EXPECT_FALSE(scheduler.PopNext(0.0).has_value());
  EXPECT_FALSE(scheduler.PopNext(123.0).has_value());
}

TEST(FifoSchedulerTest, PreservesArrivalOrder) {
  FifoScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 500.0, 10.0));
  scheduler.Push(MakeOrder(2, 1.0, 100.0, 10.0));
  scheduler.Push(MakeOrder(3, 2.0, 300.0, 10.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 1u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 2u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 3u);
}

TEST(FifoSchedulerTest, SizeTracksPushAndPop) {
  FifoScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 100.0, 10.0));
  scheduler.Push(MakeOrder(2, 1.0, 100.0, 10.0));
  EXPECT_EQ(scheduler.Size(), 2u);
  EXPECT_FALSE(scheduler.Empty());
  scheduler.PopNext(0.0);
  EXPECT_EQ(scheduler.Size(), 1u);
  scheduler.PopNext(0.0);
  EXPECT_EQ(scheduler.Size(), 0u);
  EXPECT_TRUE(scheduler.Empty());
}

TEST(FifoSchedulerTest, InterleavedPushAndPop) {
  FifoScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 100.0, 10.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 1u);
  scheduler.Push(MakeOrder(2, 1.0, 100.0, 10.0));
  scheduler.Push(MakeOrder(3, 2.0, 100.0, 10.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 2u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 3u);
  EXPECT_TRUE(scheduler.Empty());
}

TEST(FifoSchedulerTest, PopIgnoresNowArgument) {
  FifoScheduler a;
  FifoScheduler b;
  for (int i = 0; i < 5; ++i) {
    a.Push(MakeOrder(static_cast<std::uint64_t>(i), i, 100.0, 10.0));
    b.Push(MakeOrder(static_cast<std::uint64_t>(i), i, 100.0, 10.0));
  }
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(a.PopNext(0.0)->id, b.PopNext(99999.0)->id);
  }
}

TEST(FifoSchedulerTest, HandlesManyOrdersInOrder) {
  FifoScheduler scheduler;
  constexpr std::uint64_t kCount = 1000;
  for (std::uint64_t i = 0; i < kCount; ++i) {
    scheduler.Push(MakeOrder(i, static_cast<double>(i), 100.0, 10.0));
  }
  EXPECT_EQ(scheduler.Size(), kCount);
  for (std::uint64_t i = 0; i < kCount; ++i) {
    auto next = scheduler.PopNext(0.0);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->id, i);
  }
}

TEST(FifoSchedulerTest, ReportsName) {
  FifoScheduler scheduler;
  EXPECT_EQ(scheduler.Name(), "FIFO");
}

// ------------------------------ Priority -----------------------------------

TEST(PrioritySchedulerTest, StartsEmpty) {
  PriorityScheduler scheduler;
  EXPECT_TRUE(scheduler.Empty());
  EXPECT_EQ(scheduler.Size(), 0u);
}

TEST(PrioritySchedulerTest, PopOnEmptyReturnsNullopt) {
  PriorityScheduler scheduler;
  EXPECT_FALSE(scheduler.PopNext(0.0).has_value());
}

TEST(PrioritySchedulerTest, ShortestPrepPopsFirst) {
  PriorityScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 900.0, 10.0));
  scheduler.Push(MakeOrder(2, 0.0, 100.0, 10.0));
  scheduler.Push(MakeOrder(3, 0.0, 400.0, 10.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 2u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 3u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 1u);
}

TEST(PrioritySchedulerTest, EarlierArrivalWinsPrepTie) {
  PriorityScheduler scheduler;
  scheduler.Push(MakeOrder(1, 100.0, 300.0, 10.0));
  scheduler.Push(MakeOrder(2, 50.0, 300.0, 10.0));
  // Same prep and value: the earlier arrival has waited longer, pops first.
  EXPECT_EQ(scheduler.PopNext(200.0)->id, 2u);
  EXPECT_EQ(scheduler.PopNext(200.0)->id, 1u);
}

TEST(PrioritySchedulerTest, AgingCanOutweighPrepDifference) {
  PriorityScheduler scheduler;
  // Key(A) = 500 + 0.10*0    - 0 = 500
  // Key(B) = 100 + 0.10*5000 - 0 = 600  -> A first despite longer prep.
  scheduler.Push(MakeOrder(1, 0.0, 500.0, 0.0));
  scheduler.Push(MakeOrder(2, 5000.0, 100.0, 0.0));
  EXPECT_EQ(scheduler.PopNext(6000.0)->id, 1u);
  EXPECT_EQ(scheduler.PopNext(6000.0)->id, 2u);
}

TEST(PrioritySchedulerTest, HigherValueWinsWhenAllElseEqual) {
  PriorityScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 300.0, 10.0));
  scheduler.Push(MakeOrder(2, 0.0, 300.0, 60.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 2u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 1u);
}

TEST(PrioritySchedulerTest, KeyIncreasesWithPrepTime) {
  const Order base = MakeOrder(1, 100.0, 300.0, 20.0);
  Order longer = base;
  longer.prep_time = 301.0;
  EXPECT_GT(PriorityScheduler::Key(longer), PriorityScheduler::Key(base));
}

TEST(PrioritySchedulerTest, KeyDecreasesWithValue) {
  const Order base = MakeOrder(1, 100.0, 300.0, 20.0);
  Order richer = base;
  richer.value = 21.0;
  EXPECT_LT(PriorityScheduler::Key(richer), PriorityScheduler::Key(base));
}

TEST(PrioritySchedulerTest, KeySmallerForEarlierArrival) {
  const Order early = MakeOrder(1, 100.0, 300.0, 20.0);
  Order late = early;
  late.arrival_time = 200.0;
  EXPECT_LT(PriorityScheduler::Key(early), PriorityScheduler::Key(late));
}

TEST(PrioritySchedulerTest, IdenticalKeysPopInInsertionOrder) {
  PriorityScheduler scheduler;
  scheduler.Push(MakeOrder(7, 100.0, 300.0, 20.0));
  scheduler.Push(MakeOrder(8, 100.0, 300.0, 20.0));
  scheduler.Push(MakeOrder(9, 100.0, 300.0, 20.0));
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 7u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 8u);
  EXPECT_EQ(scheduler.PopNext(0.0)->id, 9u);
}

TEST(PrioritySchedulerTest, BulkPopOrderIsNonDecreasingInKey) {
  PriorityScheduler scheduler;
  // Deterministic pseudo-random orders via a small LCG.
  std::uint64_t state = 12345;
  auto next_rand = [&state]() {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>((state >> 33) % 10000) / 100.0;
  };
  constexpr int kCount = 200;
  for (int i = 0; i < kCount; ++i) {
    scheduler.Push(MakeOrder(static_cast<std::uint64_t>(i), next_rand() * 100.0,
                             50.0 + next_rand() * 10.0, next_rand()));
  }
  EXPECT_EQ(scheduler.Size(), static_cast<std::size_t>(kCount));
  double previous_key = -1e18;
  int popped = 0;
  while (auto next = scheduler.PopNext(0.0)) {
    const double key = PriorityScheduler::Key(*next);
    EXPECT_GE(key, previous_key);
    previous_key = key;
    ++popped;
  }
  EXPECT_EQ(popped, kCount);
  EXPECT_TRUE(scheduler.Empty());
}

TEST(PrioritySchedulerTest, SizeTracksPushAndPop) {
  PriorityScheduler scheduler;
  scheduler.Push(MakeOrder(1, 0.0, 100.0, 10.0));
  scheduler.Push(MakeOrder(2, 1.0, 200.0, 10.0));
  EXPECT_EQ(scheduler.Size(), 2u);
  scheduler.PopNext(0.0);
  EXPECT_EQ(scheduler.Size(), 1u);
  scheduler.PopNext(0.0);
  EXPECT_TRUE(scheduler.Empty());
}

TEST(PrioritySchedulerTest, ReportsNonFifoName) {
  PriorityScheduler scheduler;
  EXPECT_NE(scheduler.Name(), "FIFO");
  EXPECT_FALSE(scheduler.Name().empty());
}

}  // namespace
}  // namespace diner
