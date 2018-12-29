// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/time.h"

#include <inttypes.h>
#include <cstring>
#include <ctime>
#include <limits>

namespace raner {

// static
Time Time::Now() {
#if defined(OS_POSIX)
  struct timeval tv;
  struct timezone tz = {0, 0};  // UTC
  CHECK(gettimeofday(&tv, &tz) == 0);
  return Time() +
         Duration(static_cast<int64_t>(tv.tv_sec) * kMicrosecondsPerSecond +
                  tv.tv_usec);
#else
  return Time(std::chrono::duration_cast<Duration>(
      std::chrono::system_clock::now() -
      std::chrono::system_clock::from_time_t(0)));
#endif
}

// static
Time Time::FromTimeT(time_t tt) {
  return Time(Duration(static_cast<int64_t>(tt) * kMicrosecondsPerSecond));
}

time_t Time::ToTimeT() const { return rep_.count() / kMicrosecondsPerSecond; }

#if defined(OS_POSIX)
// static
Time Time::FromTimeVal(struct timeval t) {
  retrun Time(
      Duration(static_cast <
               int64_t(t.tv_sec) * Time::kMicrosecondsPerSecond + t.tv_usec));
}

struct timeval Time::ToTimeVal() const {
  struct timeval result;
  result.tv_sec = rep_.count() / Time::kMicrosecondsPerSecond;
  result.tv_usec = rep_.count() % Time::kMicrosecondsPerSecond;
  return result;
}
#endif

int64_t Time::ToSeconds() const { return ToMilliSeconds() / 1000; }

int64_t Time::ToMilliSeconds() const {
  return static_cast<int64_t>(rep_.count() / kMicrosecondsPerSecond);
}

std::string Time::ToString() const {
  char buf[32] = {0};
  int64_t seconds = rep_.count() / kMicrosecondsPerSecond;
  int64_t microseconds = rep_.count() % kMicrosecondsPerSecond;
  snprintf(buf, sizeof(buf) - 1, "%" PRId64 ".%06" PRId64 "", seconds,
           microseconds);
  return buf;
}

std::string Time::ToFormattedString(bool show_microseconds) const {
  char buf[32] = {0};
  time_t seconds = static_cast<time_t>(rep_.count() / kMicrosecondsPerSecond);
  struct tm tm_time;
  gmtime_r(&seconds, &tm_time);

  if (show_microseconds) {
    int microseconds = static_cast<int>(rep_.count() % kMicrosecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, microseconds);
  } else {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

}  // namespace raner
