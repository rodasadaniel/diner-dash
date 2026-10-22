#include "diner/simulation.h"

#include <vector>

#include <gtest/gtest.h>

#include "diner/scheduler.h"
#include "diner/workload.h"

namespace diner {
namespace {

Order MakeOrder(std::uint64_t id, double arrival, double prep,
                double value = 10.0) {
  Order order;
  order.id = id;
  order.arrival_time = arrival;
  order.prep_time = prep;
  order.value = value;
  return order;
}

// One long order in service; a long and a short order arrive while the cook
// is busy. FIFO serves the long one next; priority serves the short one.
std::vector<Order> ContentionScenario() {
  return {MakeOrder(0, 0.0, 1000.0), MakeOrder(1, 10.0, 900.0),
          MakeOrder(2, 20.0, 100.0)};
}

TEST(SimulationTest, EmptyWorkloadCompletesCleanly) {
  FifoScheduler scheduler;
  const SimulationResult result = RunSimulation({}, 6, scheduler);
  EXPECT_EQ(result.orders_arrived, 0u);
  EXPECT_EQ(result.orders_completed, 0u);
  EXPECT_EQ(result.events_processed, 0u);
  EXPECT_DOUBLE_EQ(result.avg_wait, 0.0);
  EXPECT_DOUBLE_EQ(result.makespan, 0.0);
}

TEST(SimulationTest, SingleOrderHasZeroWait) {
  FifoScheduler scheduler;
  const auto result = RunSimulation({MakeOrder(0, 50.0, 300.0)}, 1, scheduler);
  EXPECT_EQ(result.orders_completed, 1u);
  EXPECT_EQ(result.events_processed, 2u);
  EXPECT_DOUBLE_EQ(result.avg_wait, 0.0);
  EXPECT_DOUBLE_EQ(result.avg_turnaround, 300.0);
  EXPECT_DOUBLE_EQ(result.makespan, 300.0);
}

TEST(SimulationTest, SecondOrderWaitsForBusyCook) {
  FifoScheduler scheduler;
  const auto orders = {MakeOrder(0, 0.0, 100.0), MakeOrder(1, 10.0, 50.0)};
  const auto result = RunSimulation(orders, 1, scheduler);
  // Order 1 starts when order 0 finishes at t=100: wait = 90.
  EXPECT_NEAR(result.avg_wait, 45.0, 1e-9);
  EXPECT_NEAR(result.max_wait, 90.0, 1e-9);
  EXPECT_EQ(result.orders_completed, 2u);
}

TEST(SimulationTest, FifoServesWaitingOrdersInArrivalOrder) {
  FifoScheduler scheduler;
  const auto result = RunSimulation(ContentionScenario(), 1, scheduler);
  // Waits: order0 = 0, order1 = 1000-10 = 990, order2 = 1900-20 = 1880.
  EXPECT_NEAR(result.avg_wait, (0.0 + 990.0 + 1880.0) / 3.0, 1e-9);
  EXPECT_NEAR(result.max_wait, 1880.0, 1e-9);
}

TEST(SimulationTest, PriorityServesShortWaitingOrderFirst) {
  PriorityScheduler scheduler;
  const auto result = RunSimulation(ContentionScenario(), 1, scheduler);
  // Order2 (prep 100) jumps order1 (prep 900): it starts at t=1000
  // (wait 980) and finishes at t=1100; order1 starts then (wait 1090).
  EXPECT_NEAR(result.avg_wait, (0.0 + 980.0 + 1090.0) / 3.0, 1e-9);
  EXPECT_NEAR(result.max_wait, 1090.0, 1e-9);
}

TEST(SimulationTest, PriorityAvgWaitBelowFifoOnContentionScenario) {
  FifoScheduler fifo;
  PriorityScheduler priority;
  const auto fifo_result = RunSimulation(ContentionScenario(), 1, fifo);
  const auto priority_result = RunSimulation(ContentionScenario(), 1, priority);
  EXPECT_LT(priority_result.avg_wait, fifo_result.avg_wait);
}

TEST(SimulationTest, MoreCooksThanOrdersMeansZeroWaits) {
  FifoScheduler scheduler;
  std::vector<Order> orders;
  for (std::uint64_t i = 0; i < 5; ++i) {
    orders.push_back(MakeOrder(i, static_cast<double>(i) * 10.0, 500.0));
  }
  const auto result = RunSimulation(orders, 8, scheduler);
  EXPECT_DOUBLE_EQ(result.avg_wait, 0.0);
  EXPECT_DOUBLE_EQ(result.max_wait, 0.0);
  EXPECT_EQ(result.orders_completed, 5u);
}

TEST(SimulationTest, TwoCooksServeSimultaneously) {
  FifoScheduler scheduler;
  const auto orders = {MakeOrder(0, 0.0, 500.0), MakeOrder(1, 10.0, 500.0)};
  const auto result = RunSimulation(orders, 2, scheduler);
  EXPECT_DOUBLE_EQ(result.avg_wait, 0.0);
  EXPECT_NEAR(result.makespan, 510.0, 1e-9);
}

TEST(SimulationTest, ArrivalExactlyAtCompletionStartsImmediately) {
  FifoScheduler scheduler;
  const auto orders = {MakeOrder(0, 0.0, 100.0), MakeOrder(1, 100.0, 50.0)};
  const auto result = RunSimulation(orders, 1, scheduler);
  EXPECT_DOUBLE_EQ(result.avg_wait, 0.0);
  EXPECT_EQ(result.orders_completed, 2u);
}

TEST(SimulationTest, GeneratedWorkloadIsFullyServedByFifo) {
  WorkloadGenerator gen(3);
  const auto orders = gen.Generate(2000);
  FifoScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  EXPECT_EQ(result.orders_arrived, 2000u);
  EXPECT_EQ(result.orders_completed, 2000u);
}

TEST(SimulationTest, GeneratedWorkloadIsFullyServedByPriority) {
  WorkloadGenerator gen(3);
  const auto orders = gen.Generate(2000);
  PriorityScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  EXPECT_EQ(result.orders_completed, 2000u);
}

TEST(SimulationTest, EventsProcessedIsTwiceOrderCount) {
  WorkloadGenerator gen(3);
  const auto orders = gen.Generate(1500);
  FifoScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  // One arrival event + one completion event per order.
  EXPECT_EQ(result.events_processed, 3000u);
}

TEST(SimulationTest, WaitPercentilesAreOrdered) {
  WorkloadGenerator gen(13);
  const auto orders = gen.Generate(5000);
  FifoScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  EXPECT_GE(result.avg_wait, 0.0);
  EXPECT_LE(result.p50_wait, result.p95_wait);
  EXPECT_LE(result.p95_wait, result.max_wait);
}

TEST(SimulationTest, PriorityBeatsFifoAverageWaitOnGeneratedWorkload) {
  WorkloadGenerator gen(42);
  const auto orders = gen.Generate(20000);
  FifoScheduler fifo;
  PriorityScheduler priority;
  const auto fifo_result = RunSimulation(orders, 6, fifo);
  const auto priority_result = RunSimulation(orders, 6, priority);
  EXPECT_LT(priority_result.avg_wait, fifo_result.avg_wait);
}

TEST(SimulationTest, SameInputProducesIdenticalResults) {
  WorkloadGenerator gen(17);
  const auto orders = gen.Generate(3000);
  FifoScheduler first;
  FifoScheduler second;
  const auto a = RunSimulation(orders, 6, first);
  const auto b = RunSimulation(orders, 6, second);
  EXPECT_DOUBLE_EQ(a.avg_wait, b.avg_wait);
  EXPECT_DOUBLE_EQ(a.makespan, b.makespan);
  EXPECT_DOUBLE_EQ(a.avg_turnaround, b.avg_turnaround);
  EXPECT_EQ(a.events_processed, b.events_processed);
}

TEST(SimulationTest, MoreCooksReduceAverageWait) {
  WorkloadGenerator gen(23);
  const auto orders = gen.Generate(5000);
  FifoScheduler one_cook;
  FifoScheduler six_cooks;
  const auto crowded = RunSimulation(orders, 1, one_cook);
  const auto staffed = RunSimulation(orders, 6, six_cooks);
  EXPECT_GT(crowded.avg_wait, staffed.avg_wait);
}

TEST(SimulationTest, ThroughputIsPositiveForNontrivialRun) {
  WorkloadGenerator gen(29);
  const auto orders = gen.Generate(1000);
  FifoScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  EXPECT_GT(result.throughput_per_hour, 0.0);
  EXPECT_GT(result.events_per_second, 0.0);
  EXPECT_GT(result.wall_seconds, 0.0);
}

TEST(SimulationTest, ResultRecordsSchedulerName) {
  FifoScheduler fifo;
  PriorityScheduler priority;
  const auto orders = {MakeOrder(0, 0.0, 100.0)};
  EXPECT_EQ(RunSimulation(orders, 1, fifo).scheduler_name, "FIFO");
  EXPECT_EQ(RunSimulation(orders, 1, priority).scheduler_name,
            "Priority (SPT + aging + value)");
}

TEST(SimulationTest, TurnaroundEqualsWaitPlusPrepOnAverage) {
  WorkloadGenerator gen(31);
  const auto orders = gen.Generate(4000);
  double prep_sum = 0.0;
  for (const Order& order : orders) prep_sum += order.prep_time;
  const double avg_prep = prep_sum / static_cast<double>(orders.size());
  PriorityScheduler scheduler;
  const auto result = RunSimulation(orders, 6, scheduler);
  EXPECT_NEAR(result.avg_turnaround, result.avg_wait + avg_prep, 1e-6);
}

}  // namespace
}  // namespace diner
