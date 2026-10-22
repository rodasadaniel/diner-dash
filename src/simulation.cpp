#include "diner/simulation.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <vector>

#include "diner/stats.h"

namespace diner {
namespace {

enum class EventType { kArrival, kCompletion };

struct Event {
  double time;
  EventType type;
  std::size_t order_index;  // position in the orders vector (ids need not be dense)
  std::uint64_t seq;        // insertion order; makes same-instant order stable
};

// Min-heap by time. At the same instant, completions are processed before
// arrivals so a freed cook is available to the arriving order.
struct EventCompare {
  bool operator()(const Event& a, const Event& b) const {
    if (a.time != b.time) return a.time > b.time;
    if (a.type != b.type) return a.type == EventType::kArrival;
    return a.seq > b.seq;
  }
};

class Engine {
 public:
  Engine(const std::vector<Order>& orders, int cooks, Scheduler& scheduler)
      : orders_(orders), cooks_(cooks), scheduler_(scheduler) {}

  SimulationResult Run() {
    const auto wall_start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < orders_.size(); ++i) {
      PushEvent(Event{orders_[i].arrival_time, EventType::kArrival, i, 0});
      if (i == 0 || orders_[i].arrival_time < first_arrival_) {
        first_arrival_ = orders_[i].arrival_time;
      }
    }

    while (!events_.empty()) {
      const Event event = events_.top();
      events_.pop();
      ++events_processed_;

      const Order& order = orders_[event.order_index];
      if (event.type == EventType::kArrival) {
        OnArrival(order, event.order_index, event.time);
      } else {
        OnCompletion(order, event.time);
      }
    }

    const auto wall_end = std::chrono::steady_clock::now();

    SimulationResult result;
    result.scheduler_name = scheduler_.Name();
    result.orders_arrived = orders_.size();
    result.orders_completed = completed_;
    result.events_processed = events_processed_;
    result.makespan = completed_ == 0 ? 0.0 : last_completion_ - first_arrival_;
    result.avg_wait = waits_.Mean();
    result.p50_wait = waits_.Percentile(50.0);
    result.p95_wait = waits_.Percentile(95.0);
    result.max_wait = waits_.Max();
    result.avg_turnaround = turnaround_.Mean();
    result.throughput_per_hour =
        result.makespan > 0.0
            ? static_cast<double>(completed_) / result.makespan * 3600.0
            : 0.0;
    result.wall_seconds =
        std::chrono::duration<double>(wall_end - wall_start).count();
    result.events_per_second =
        result.wall_seconds > 0.0
            ? static_cast<double>(events_processed_) / result.wall_seconds
            : 0.0;
    result.orders_per_second =
        result.wall_seconds > 0.0
            ? static_cast<double>(completed_) / result.wall_seconds
            : 0.0;
    return result;
  }

 private:
  void PushEvent(Event event) {
    event.seq = next_seq_++;
    events_.push(event);
  }

  void StartService(const Order& order, std::size_t order_index, double now) {
    ++busy_;
    waits_.Add(now - order.arrival_time);
    PushEvent(Event{now + order.prep_time, EventType::kCompletion, order_index, 0});
  }

  void OnArrival(const Order& order, std::size_t order_index, double now) {
    if (busy_ < cooks_) {
      StartService(order, order_index, now);
    } else {
      scheduler_.Push(order);
      waiting_index_[order.id] = order_index;
    }
  }

  void OnCompletion(const Order& order, double now) {
    --busy_;
    ++completed_;
    turnaround_.Add(now - order.arrival_time);
    last_completion_ = std::max(last_completion_, now);
    if (std::optional<Order> next = scheduler_.PopNext(now)) {
      StartService(*next, waiting_index_[next->id], now);
    }
  }

  const std::vector<Order>& orders_;
  int cooks_;
  Scheduler& scheduler_;
  std::unordered_map<std::uint64_t, std::size_t> waiting_index_;

  std::priority_queue<Event, std::vector<Event>, EventCompare> events_;
  std::uint64_t next_seq_ = 0;
  std::uint64_t events_processed_ = 0;
  std::uint64_t completed_ = 0;
  int busy_ = 0;
  double first_arrival_ = 0.0;
  double last_completion_ = 0.0;
  Distribution waits_;
  RunningStats turnaround_;
};

}  // namespace

SimulationResult RunSimulation(const std::vector<Order>& orders, int cooks,
                               Scheduler& scheduler) {
  Engine engine(orders, cooks, scheduler);
  return engine.Run();
}

}  // namespace diner
