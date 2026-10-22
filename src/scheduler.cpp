#include "diner/scheduler.h"

namespace diner {

// ------------------------------- FIFO --------------------------------------

void FifoScheduler::Push(const Order& order) { queue_.push_back(order); }

std::optional<Order> FifoScheduler::PopNext(double /*now*/) {
  if (queue_.empty()) return std::nullopt;
  Order next = queue_.front();
  queue_.pop_front();
  return next;
}

// ----------------------------- Priority ------------------------------------

double PriorityScheduler::Key(const Order& order) {
  return order.prep_time + kAgingWeight * order.arrival_time -
         kValueWeight * order.value;
}

void PriorityScheduler::Push(const Order& order) {
  heap_.push(Entry{Key(order), next_seq_++, order});
}

std::optional<Order> PriorityScheduler::PopNext(double /*now*/) {
  if (heap_.empty()) return std::nullopt;
  Order next = heap_.top().order;
  heap_.pop();
  return next;
}

}  // namespace diner
