#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "diner/order.h"

namespace diner {

// Strategy interface for the waiting room: decides which queued order a free
// cook picks up next. Implementations are stateful and single-use; create a
// fresh scheduler per simulation run.
class Scheduler {
 public:
  virtual ~Scheduler() = default;

  // Enqueue an order that could not be served immediately.
  virtual void Push(const Order& order) = 0;

  // Remove and return the next order to serve at simulated time `now`.
  // Returns std::nullopt when the waiting room is empty.
  virtual std::optional<Order> PopNext(double now) = 0;

  virtual bool Empty() const = 0;
  virtual std::size_t Size() const = 0;
  virtual std::string Name() const = 0;
};

// Baseline strategy: strict first-in, first-out.
class FifoScheduler : public Scheduler {
 public:
  void Push(const Order& order) override;
  std::optional<Order> PopNext(double now) override;
  bool Empty() const override { return queue_.empty(); }
  std::size_t Size() const override { return queue_.size(); }
  std::string Name() const override { return "FIFO"; }

 private:
  std::deque<Order> queue_;
};

// Priority strategy: shortest-processing-time first, tempered by an aging
// term (so long-waiting orders steadily move up) and an order-value term
// (higher-value orders get a modest boost).
//
// The score minimised at dispatch time `now` is
//
//     score(order, now) = prep_time - kAgingWeight * (now - arrival_time)
//                         - kValueWeight * value
//
// Because `now` is the same for every candidate at a given dispatch, this is
// equivalent to minimising the static key
//
//     Key(order) = prep_time + kAgingWeight * arrival_time
//                  - kValueWeight * value
//
// which can be computed once, when the order is enqueued. The waiting room is
// therefore a plain binary heap ordered by Key, giving O(log n) push/pop
// while still honouring true linear aging.
class PriorityScheduler : public Scheduler {
 public:
  static constexpr double kAgingWeight = 0.5;
  static constexpr double kValueWeight = 1.0;

  // Static heap key described above. Exposed for testing and tuning.
  static double Key(const Order& order);

  void Push(const Order& order) override;
  std::optional<Order> PopNext(double now) override;
  bool Empty() const override { return heap_.empty(); }
  std::size_t Size() const override { return heap_.size(); }
  std::string Name() const override { return "Priority (SPT + aging + value)"; }

 private:
  struct Entry {
    double key;
    std::uint64_t seq;  // insertion order, breaks ties FIFO-style
    Order order;
  };
  struct EntryCompare {
    bool operator()(const Entry& a, const Entry& b) const {
      if (a.key != b.key) return a.key > b.key;  // smallest key on top
      return a.seq > b.seq;
    }
  };

  std::priority_queue<Entry, std::vector<Entry>, EntryCompare> heap_;
  std::uint64_t next_seq_ = 0;
};

}  // namespace diner
