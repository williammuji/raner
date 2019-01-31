// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_BASE_TIME_H_
#define RANER_BASE_TIME_H_

#include <sys/time.h>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <ctime>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

namespace raner {

typedef std::chrono::microseconds Duration;

class Time {
 public:
  static constexpr int64_t kMicrosecondsPerSecond = 1000 * 1000;

  constexpr Time() : rep_(Duration::zero()) {}

  static Time UnixEpoch() { return Time(); }
  static Time Now();

  static Time FromTimeT(time_t tt);
  time_t ToTimeT() const;

  int64_t ToSeconds() const;
  int64_t ToMilliSeconds() const;

#if defined(OS_POSIX)
  static Time FromTimeVal(struct timeval t);
  struct timeval ToTimeVal() const;
#endif

  std::string ToString() const;
  std::string ToFormattedString(bool show_microseconds) const;

  // Assignment operators.
  Time& operator+=(Duration d) {
    rep_ += d;
    return *this;
  }
  Time& operator-=(Duration d) {
    rep_ -= d;
    return *this;
  }

  bool IsInitialized() const { return rep_ != Duration::zero(); }

 private:
  friend constexpr bool operator<(Time lhs, Time rhs);
  friend constexpr bool operator==(Time lhs, Time rhs);
  friend Duration operator-(Time lhs, Time rhs);

  constexpr explicit Time(Duration rep) : rep_(rep) {}
  Duration rep_;
};

// Relational Operators
constexpr bool operator<(Time lhs, Time rhs) { return lhs.rep_ < rhs.rep_; }
constexpr bool operator>(Time lhs, Time rhs) { return rhs < lhs; }
constexpr bool operator>=(Time lhs, Time rhs) { return !(lhs < rhs); }
constexpr bool operator<=(Time lhs, Time rhs) { return !(rhs < lhs); }
constexpr bool operator==(Time lhs, Time rhs) { return lhs.rep_ == rhs.rep_; }
constexpr bool operator!=(Time lhs, Time rhs) { return !(lhs == rhs); }

// Additive Operators
inline Time operator+(Time lhs, Duration rhs) { return lhs += rhs; }
inline Time operator+(Duration lhs, Time rhs) { return rhs += lhs; }
inline Time operator-(Time lhs, Duration rhs) { return lhs -= rhs; }
inline Duration operator-(Time lhs, Time rhs) { return lhs.rep_ - rhs.rep_; }

inline int64_t durationSeconds(Duration d) {
  return std::chrono::duration_cast<std::chrono::seconds>(d).count();
}

}  // namespace raner

#endif  // RANER_BASE_TIME_H_
