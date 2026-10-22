#pragma once

#include <cstdint>

namespace diner {

// A single restaurant order flowing through the simulation.
//
// Times are in simulated seconds. `prep_time` is the amount of cook time the
// order needs once it reaches a station; `value` is the order's price in
// dollars and is used by the priority scheduler as a secondary signal.
struct Order {
  std::uint64_t id = 0;
  double arrival_time = 0.0;
  double prep_time = 0.0;
  double value = 0.0;
};

}  // namespace diner
