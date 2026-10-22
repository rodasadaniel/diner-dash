#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "diner/scheduler.h"
#include "diner/simulation.h"
#include "diner/workload.h"

namespace {

void PrintResult(const diner::SimulationResult& r) {
  std::cout << r.scheduler_name << '\n';
  std::cout << "  orders completed     : " << r.orders_completed << " / "
            << r.orders_arrived << '\n';
  std::cout << "  events processed     : " << r.events_processed << '\n';
  std::cout << "  avg wait             : " << std::fixed << std::setprecision(2)
            << r.avg_wait << " s\n";
  std::cout << "  p50 / p95 / max wait : " << r.p50_wait << " / " << r.p95_wait
            << " / " << r.max_wait << " s\n";
  std::cout << "  avg turnaround       : " << r.avg_turnaround << " s\n";
  std::cout << "  makespan             : " << r.makespan << " s\n";
  std::cout << "  throughput           : " << std::setprecision(1)
            << r.throughput_per_hour << " orders/sim-hour\n";
  std::cout << "  execution time       : " << std::setprecision(3)
            << r.wall_seconds << " s (" << std::setprecision(0)
            << r.events_per_second << " events/s, " << r.orders_per_second
            << " orders/s)\n";
}

double PercentDelta(double candidate, double baseline) {
  return baseline == 0.0 ? 0.0 : (candidate - baseline) / baseline * 100.0;
}

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t order_count = 100000;
  std::uint64_t seed = 42;
  int cooks = 6;
  std::string mode = "both";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next_value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << arg << '\n';
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "--orders") {
      order_count = std::stoull(next_value());
    } else if (arg == "--seed") {
      seed = std::stoull(next_value());
    } else if (arg == "--cooks") {
      cooks = std::stoi(next_value());
    } else if (arg == "--scheduler") {
      mode = next_value();
    } else if (arg == "--help") {
      std::cout << "Usage: diner_dash [--orders N] [--seed S] [--cooks K]\n"
                   "                  [--scheduler both|fifo|priority]\n";
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << '\n';
      return 2;
    }
  }

  diner::WorkloadGenerator generator(seed);
  const std::vector<diner::Order> orders = generator.Generate(order_count);

  std::cout << "=== Diner Dash Benchmark ===\n";
  std::cout << "orders=" << order_count << " cooks=" << cooks << " seed=" << seed
            << "\n\n";

  diner::SimulationResult fifo_result;
  diner::SimulationResult priority_result;
  bool have_fifo = false;
  bool have_priority = false;

  if (mode == "both" || mode == "fifo") {
    diner::FifoScheduler scheduler;
    fifo_result = diner::RunSimulation(orders, cooks, scheduler);
    PrintResult(fifo_result);
    std::cout << '\n';
    have_fifo = true;
  }
  if (mode == "both" || mode == "priority") {
    diner::PriorityScheduler scheduler;
    priority_result = diner::RunSimulation(orders, cooks, scheduler);
    PrintResult(priority_result);
    std::cout << '\n';
    have_priority = true;
  }

  if (have_fifo && have_priority) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Priority vs FIFO:\n";
    std::cout << "  avg wait delta       : "
              << PercentDelta(priority_result.avg_wait, fifo_result.avg_wait)
              << " %\n";
    std::cout << "  p95 wait delta       : "
              << PercentDelta(priority_result.p95_wait, fifo_result.p95_wait)
              << " %\n";
    std::cout << "  avg turnaround delta : "
              << PercentDelta(priority_result.avg_turnaround,
                              fifo_result.avg_turnaround)
              << " %\n";
    std::cout << "  throughput delta     : "
              << PercentDelta(priority_result.throughput_per_hour,
                              fifo_result.throughput_per_hour)
              << " %\n";
    std::cout << "  execution time delta : "
              << PercentDelta(priority_result.wall_seconds,
                              fifo_result.wall_seconds)
              << " %\n";
  }
  return 0;
}
