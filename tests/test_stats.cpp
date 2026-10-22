#include "diner/stats.h"

#include <cmath>

#include <gtest/gtest.h>

namespace diner {
namespace {

// --------------------------- RunningStats ----------------------------------

TEST(RunningStatsTest, EmptyHasZeroCountMeanAndVariance) {
  RunningStats stats;
  EXPECT_EQ(stats.Count(), 0u);
  EXPECT_DOUBLE_EQ(stats.Mean(), 0.0);
  EXPECT_DOUBLE_EQ(stats.Variance(), 0.0);
  EXPECT_DOUBLE_EQ(stats.StdDev(), 0.0);
  EXPECT_DOUBLE_EQ(stats.Sum(), 0.0);
}

TEST(RunningStatsTest, SingleValue) {
  RunningStats stats;
  stats.Add(7.5);
  EXPECT_EQ(stats.Count(), 1u);
  EXPECT_DOUBLE_EQ(stats.Mean(), 7.5);
  EXPECT_DOUBLE_EQ(stats.Variance(), 0.0);
  EXPECT_DOUBLE_EQ(stats.Min(), 7.5);
  EXPECT_DOUBLE_EQ(stats.Max(), 7.5);
  EXPECT_DOUBLE_EQ(stats.Sum(), 7.5);
}

TEST(RunningStatsTest, KnownDatasetMeanAndVariance) {
  RunningStats stats;
  for (double x : {1.0, 2.0, 3.0, 4.0, 5.0}) stats.Add(x);
  EXPECT_EQ(stats.Count(), 5u);
  EXPECT_DOUBLE_EQ(stats.Mean(), 3.0);
  EXPECT_DOUBLE_EQ(stats.Variance(), 2.0);  // population variance
  EXPECT_NEAR(stats.StdDev(), std::sqrt(2.0), 1e-12);
  EXPECT_DOUBLE_EQ(stats.Sum(), 15.0);
}

TEST(RunningStatsTest, MinMaxTrackedAcrossAdds) {
  RunningStats stats;
  for (double x : {4.0, -2.0, 9.0, 0.0, 9.0}) stats.Add(x);
  EXPECT_DOUBLE_EQ(stats.Min(), -2.0);
  EXPECT_DOUBLE_EQ(stats.Max(), 9.0);
}

TEST(RunningStatsTest, NegativeValuesMean) {
  RunningStats stats;
  for (double x : {-10.0, -20.0, -30.0}) stats.Add(x);
  EXPECT_DOUBLE_EQ(stats.Mean(), -20.0);
}

TEST(RunningStatsTest, ConstantSeriesHasZeroVariance) {
  RunningStats stats;
  for (int i = 0; i < 100; ++i) stats.Add(42.0);
  EXPECT_DOUBLE_EQ(stats.Mean(), 42.0);
  EXPECT_DOUBLE_EQ(stats.Variance(), 0.0);
}

TEST(RunningStatsTest, ClearResetsEverything) {
  RunningStats stats;
  stats.Add(1.0);
  stats.Add(2.0);
  stats.Clear();
  EXPECT_EQ(stats.Count(), 0u);
  EXPECT_DOUBLE_EQ(stats.Mean(), 0.0);
  stats.Add(5.0);
  EXPECT_DOUBLE_EQ(stats.Mean(), 5.0);
  EXPECT_DOUBLE_EQ(stats.Min(), 5.0);
}

TEST(RunningStatsTest, LargeOffsetValuesRemainAccurate) {
  // Welford's algorithm should not lose the small spread to cancellation.
  RunningStats stats;
  stats.Add(1e9 + 1.0);
  stats.Add(1e9 + 2.0);
  stats.Add(1e9 + 3.0);
  EXPECT_NEAR(stats.Mean(), 1e9 + 2.0, 1e-3);
  EXPECT_NEAR(stats.Variance(), 2.0 / 3.0, 1e-6);
}

// --------------------------- Distribution ----------------------------------

TEST(DistributionTest, EmptyPercentilesAreZero) {
  Distribution dist;
  EXPECT_EQ(dist.Count(), 0u);
  EXPECT_DOUBLE_EQ(dist.Mean(), 0.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(50.0), 0.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(95.0), 0.0);
  EXPECT_DOUBLE_EQ(dist.Min(), 0.0);
  EXPECT_DOUBLE_EQ(dist.Max(), 0.0);
}

TEST(DistributionTest, SingleSampleAllPercentiles) {
  Distribution dist;
  dist.Add(13.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(0.0), 13.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(50.0), 13.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(100.0), 13.0);
  EXPECT_DOUBLE_EQ(dist.Median(), 13.0);
}

TEST(DistributionTest, MedianOfOneToHundred) {
  Distribution dist;
  for (int i = 1; i <= 100; ++i) dist.Add(static_cast<double>(i));
  EXPECT_DOUBLE_EQ(dist.Median(), 50.5);
  EXPECT_DOUBLE_EQ(dist.Mean(), 50.5);
}

TEST(DistributionTest, PercentileEndpointsAreMinAndMax) {
  Distribution dist;
  for (double x : {30.0, 10.0, 50.0, 20.0, 40.0}) dist.Add(x);
  EXPECT_DOUBLE_EQ(dist.Percentile(0.0), 10.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(100.0), 50.0);
  EXPECT_DOUBLE_EQ(dist.Min(), 10.0);
  EXPECT_DOUBLE_EQ(dist.Max(), 50.0);
}

TEST(DistributionTest, PercentileInterpolatesBetweenRanks) {
  Distribution dist;
  dist.Add(0.0);
  dist.Add(10.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(25.0), 2.5);
  EXPECT_DOUBLE_EQ(dist.Percentile(75.0), 7.5);
}

TEST(DistributionTest, UnsortedInputHandled) {
  Distribution dist;
  for (double x : {5.0, 1.0, 4.0, 2.0, 3.0}) dist.Add(x);
  EXPECT_DOUBLE_EQ(dist.Median(), 3.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(0.0), 1.0);
  EXPECT_DOUBLE_EQ(dist.Percentile(100.0), 5.0);
}

TEST(DistributionTest, MeanAndStdDevOfKnownSet) {
  Distribution dist;
  for (double x : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}) dist.Add(x);
  EXPECT_DOUBLE_EQ(dist.Mean(), 5.0);
  EXPECT_NEAR(dist.StdDev(), 2.0, 1e-12);  // population stddev
}

TEST(DistributionTest, PercentilesAreMonotonic) {
  Distribution dist;
  for (int i = 0; i < 1000; ++i) dist.Add(static_cast<double>(i % 97));
  EXPECT_LE(dist.Percentile(10.0), dist.Percentile(50.0));
  EXPECT_LE(dist.Percentile(50.0), dist.Percentile(90.0));
  EXPECT_LE(dist.Percentile(90.0), dist.Percentile(100.0));
}

TEST(DistributionTest, CountAndClear) {
  Distribution dist;
  dist.Add(1.0);
  dist.Add(2.0);
  EXPECT_EQ(dist.Count(), 2u);
  dist.Clear();
  EXPECT_EQ(dist.Count(), 0u);
  EXPECT_DOUBLE_EQ(dist.Mean(), 0.0);
}

}  // namespace
}  // namespace diner
