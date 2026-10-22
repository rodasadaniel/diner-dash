#include "diner/stats.h"

#include <algorithm>
#include <cmath>

namespace diner {

// ---------------------------- RunningStats ---------------------------------

void RunningStats::Add(double x) {
  if (count_ == 0) {
    min_ = x;
    max_ = x;
  } else {
    min_ = std::min(min_, x);
    max_ = std::max(max_, x);
  }
  ++count_;
  sum_ += x;
  const double delta = x - mean_;
  mean_ += delta / static_cast<double>(count_);
  m2_ += delta * (x - mean_);
}

void RunningStats::Clear() { *this = RunningStats(); }

double RunningStats::Mean() const { return count_ == 0 ? 0.0 : mean_; }

double RunningStats::Variance() const {
  return count_ == 0 ? 0.0 : m2_ / static_cast<double>(count_);
}

double RunningStats::StdDev() const { return std::sqrt(Variance()); }

// ---------------------------- Distribution ---------------------------------

void Distribution::Add(double x) {
  samples_.push_back(x);
  running_.Add(x);
}

void Distribution::Clear() {
  samples_.clear();
  running_.Clear();
}

double Distribution::Mean() const { return running_.Mean(); }
double Distribution::StdDev() const { return running_.StdDev(); }
double Distribution::Min() const { return samples_.empty() ? 0.0 : running_.Min(); }
double Distribution::Max() const { return samples_.empty() ? 0.0 : running_.Max(); }

double Distribution::Percentile(double p) const {
  if (samples_.empty()) return 0.0;
  std::vector<double> sorted = samples_;
  std::sort(sorted.begin(), sorted.end());
  if (sorted.size() == 1) return sorted.front();

  const double rank = (p / 100.0) * static_cast<double>(sorted.size() - 1);
  const std::size_t lo = static_cast<std::size_t>(std::floor(rank));
  const std::size_t hi = static_cast<std::size_t>(std::ceil(rank));
  if (lo == hi) return sorted[lo];
  const double frac = rank - static_cast<double>(lo);
  return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

}  // namespace diner
