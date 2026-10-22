#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "diner/order.h"

namespace diner {

// Seeded, reproducible order-arrival generator.
//
// Arrivals follow a Poisson process (exponential inter-arrival gaps).
// Preparation times are a two-component mixture modelling a real kitchen:
// most orders are standard dishes, a minority are large/slow ones. Order
// value correlates loosely with preparation time. The same seed always
// produces the identical workload, which is what makes the FIFO vs. priority
// comparison fair: both schedulers face exactly the same orders.
class WorkloadGenerator {
 public:
  explicit WorkloadGenerator(std::uint64_t seed) : rng_(seed) {}

  std::vector<Order> Generate(std::uint64_t order_count);

  // Workload shape constants (seconds / dollars), exposed for tests.
  static constexpr double kMeanInterarrival = 99.0;
  static constexpr double kShortPrepMin = 120.0;
  static constexpr double kShortPrepMax = 420.0;
  static constexpr double kLongPrepMin = 600.0;
  static constexpr double kLongPrepMax = 1500.0;
  static constexpr double kLongOrderProbability = 0.30;
  static constexpr double kMinValue = 8.0;
  static constexpr double kMaxValue = 80.0;

 private:
  std::mt19937_64 rng_;
};

}  // namespace diner
