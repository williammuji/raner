#ifndef RANER_UTIL_H_
#define RANER_UTIL_H_

namespace raner {
void SleepUsec(int64_t usec) {
  struct timespec ts = {0, 0};
  ts.tv_sec = static_cast<time_t>(usec / Time::kMicrosecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Time::kMicrosecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}
}  // namespace raner

#endif
