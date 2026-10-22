#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diner/order.h"
#include "diner/scheduler.h"

namespace diner {

// Outcome of one simulation run. Times are simulated seconds unless the
// field name says otherwise.
struct SimulationResult {
  std::string scheduler_name;
  std::uint64_t orders_arrived = 0;
  std::uint64_t orders_completed = 0;
  std::uint64_t events_processed = 0;

  double makespan = 0.0;          // last completion - first arrival
  double avg_wait = 0.0;          // arrival -> service start
  double p50_wait = 0.0;
  double p95_wait = 0.0;
  double max_wait = 0.0;
  double avg_turnaround = 0.0;    // arrival -> completion
  double throughput_per_hour = 0.0;  // completed orders per simulated hour

  double wall_seconds = 0.0;         // measured execution time of the run
  double events_per_second = 0.0;    // events_processed / wall_seconds
  double orders_per_second = 0.0;    // orders_completed / wall_seconds
};

// Runs a discrete-event simulation of the kitchen over `orders` with
// `cooks` identical parallel stations, dispatching waiting orders through
// `scheduler` (which must be fresh). The engine is work-conserving: a cook
// never idles while orders are waiting.
//
// `orders` does not need to be sorted; events are ordered internally.
SimulationResult RunSimulation(const std::vector<Order>& orders, int cooks,
                               Scheduler& scheduler);

}  // namespace diner
