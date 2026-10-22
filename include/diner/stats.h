#pragma once

#include <cstddef>
#include <vector>

namespace diner {

// Online mean / variance / min / max using Welford's algorithm. Stores no
// samples, so it is safe for unbounded streams.
class RunningStats {
 public:
  void Add(double x);
  void Clear();

  std::size_t Count() const { return count_; }
  double Sum() const { return sum_; }
  double Mean() const;
  double Variance() const;  // population variance
  double StdDev() const;
  double Min() const { return min_; }
  double Max() const { return max_; }

 private:
  std::size_t count_ = 0;
  double sum_ = 0.0;
  double mean_ = 0.0;
  double m2_ = 0.0;
  double min_ = 0.0;
  double max_ = 0.0;
};

// Sample-storing statistics used for wait-time distributions, where
// percentiles matter. Percentile uses linear interpolation between closest
// ranks (the "type 7" method, matching numpy's default).
class Distribution {
 public:
  void Add(double x);
  void Clear();

  std::size_t Count() const { return samples_.size(); }
  double Mean() const;
  double StdDev() const;
  double Min() const;
  double Max() const;
  double Percentile(double p) const;  // p in [0, 100]
  double Median() const { return Percentile(50.0); }

 private:
  std::vector<double> samples_;
  RunningStats running_;
};

}  // namespace diner
