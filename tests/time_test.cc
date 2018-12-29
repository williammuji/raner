#include "raner/time.h"

#include <chrono>  // NOLINT(build/c++11)
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace raner {
namespace {

TEST(TimeTest, Zero) {
  Time t0 = Time::UnixEpoch();
  EXPECT_EQ(t0, Time());
  Time t1 = Time::FromTimeT(0);
  EXPECT_EQ(t1, Time());
}

TEST(TimeTest, ValueSemantics) {
  Time a;      // Default construction
  Time b = a;  // Copy construction
  EXPECT_EQ(a, b);
  Time c(a);  // Copy construction (again)
  EXPECT_EQ(a, b);
  EXPECT_EQ(a, c);
  EXPECT_EQ(b, c);
  b = c;  // Assignment
  EXPECT_EQ(a, b);
  EXPECT_EQ(a, c);
  EXPECT_EQ(b, c);
}

TEST(TimeTest, AdditiveOperators) {
  const Duration d(1);  // 1 microsecond
  const Time t0;
  const Time t1 = t0 + d;

  EXPECT_EQ(d, t1 - t0);
  EXPECT_EQ(-d, t0 - t1);
  EXPECT_EQ(t0, t1 - d);

  Time t(t0);
  EXPECT_EQ(t0, t);
  t += d;
  EXPECT_EQ(t0 + d, t);
  EXPECT_EQ(d, t - t0);
  t -= d;
  EXPECT_EQ(t0, t);

  // Tests overflow between subseconds and seconds.
  t = Time::UnixEpoch();
  t += std::chrono::milliseconds(500);
  EXPECT_EQ(Time::UnixEpoch() + std::chrono::milliseconds(500), t);
  t += std::chrono::milliseconds(600);
  EXPECT_EQ(Time::UnixEpoch() + std::chrono::milliseconds(1100), t);
  t -= std::chrono::milliseconds(600);
  EXPECT_EQ(Time::UnixEpoch() + std::chrono::milliseconds(500), t);
  t -= std::chrono::milliseconds(500);
  EXPECT_EQ(Time::UnixEpoch(), t);
}

TEST(TimeTest, RelationalOperators) {
  Time t1 = Time::FromTimeT(0);
  Time t2 = Time::FromTimeT(1);
  Time t3 = Time::FromTimeT(2);

  EXPECT_EQ(Time(), t1);
  EXPECT_EQ(t1, t1);
  EXPECT_EQ(t2, t2);
  EXPECT_EQ(t3, t3);

  EXPECT_LT(t1, t2);
  EXPECT_LT(t2, t3);
  EXPECT_LT(t1, t3);

  EXPECT_LE(t1, t1);
  EXPECT_LE(t1, t2);
  EXPECT_LE(t2, t2);
  EXPECT_LE(t2, t3);
  EXPECT_LE(t3, t3);
  EXPECT_LE(t1, t3);

  EXPECT_GT(t2, t1);
  EXPECT_GT(t3, t2);
  EXPECT_GT(t3, t1);

  EXPECT_GE(t2, t2);
  EXPECT_GE(t2, t1);
  EXPECT_GE(t3, t3);
  EXPECT_GE(t3, t2);
  EXPECT_GE(t1, t1);
  EXPECT_GE(t3, t1);
}

TEST(TimeTest, Range) {
  // The API's documented range is +/- 100 billion years.
  const Duration range =
      std::chrono::hours(24) * static_cast<int64_t>(365.2425 * 100000000000);

  // Arithmetic and comparison still works at +/-range around base values.
  Time bases[2] = {Time::UnixEpoch(), Time::Now()};
  for (const auto base : bases) {
    Time bottom = base - range;
    EXPECT_GT(bottom, bottom - std::chrono::microseconds(1));
    EXPECT_LT(bottom, bottom + std::chrono::microseconds(1));
    Time top = base + range;
    EXPECT_GT(top, top - std::chrono::microseconds(1));
    EXPECT_LT(top, top + std::chrono::microseconds(1));
    Duration full_range = 2 * range;
    EXPECT_EQ(full_range, top - bottom);
    EXPECT_EQ(-full_range, bottom - top);
  }
}

}  // namespace
}  // namespace raner
