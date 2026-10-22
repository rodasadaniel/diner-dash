#include "diner/workload.h"

#include <algorithm>

namespace diner {

std::vector<Order> WorkloadGenerator::Generate(std::uint64_t order_count) {
  std::vector<Order> orders;
  orders.reserve(order_count);

  std::exponential_distribution<double> interarrival(1.0 / kMeanInterarrival);
  std::uniform_real_distribution<double> short_prep(kShortPrepMin, kShortPrepMax);
  std::uniform_real_distribution<double> long_prep(kLongPrepMin, kLongPrepMax);
  std::bernoulli_distribution is_long(kLongOrderProbability);
  std::uniform_real_distribution<double> short_value(8.0, 25.0);
  std::uniform_real_distribution<double> long_value(25.0, 80.0);

  double now = 0.0;
  for (std::uint64_t i = 0; i < order_count; ++i) {
    now += interarrival(rng_);
    Order order;
    order.id = i;
    order.arrival_time = now;
    if (is_long(rng_)) {
      order.prep_time = long_prep(rng_);
      order.value = long_value(rng_);
    } else {
      order.prep_time = short_prep(rng_);
      order.value = short_value(rng_);
    }
    orders.push_back(order);
  }
  return orders;
}

}  // namespace diner
