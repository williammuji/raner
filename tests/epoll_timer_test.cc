#include "raner/epoll_timer.h"

#include "raner/epoll_server.h"

#include <gtest/gtest.h>

namespace raner {
namespace {

class TestTimerDelegate {
 public:
  TestTimerDelegate() : fired_(false) {}

  void OnTimer() { fired_ = true; }

  bool fired() const { return fired_; }

 private:
  bool fired_;
};
TestTimerDelegate testTimerDelegate;

// Unlike the full MockEpollServer, this only lies about the time but lets
// fd events operate normally.  Usefully when interacting with real backends
// but wanting to skip forward in time to trigger timeouts.
class FakeTimeEpollServer : public EpollServer {
 public:
  FakeTimeEpollServer() : now_in_usec_(0) {}
  ~FakeTimeEpollServer() override {}

  virtual Time Now() const {
    return Time::UnixEpoch() + Duration(now_in_usec_);
  }

  // Replaces the EpollServer NowInUsec.
  int64_t NowInUsec() const override { return now_in_usec_; }

  void set_now_in_usec(int64_t nius) { now_in_usec_ = nius; }

  // Advances the virtual 'now' by advancement_usec.
  void AdvanceBy(int64_t advancement_usec) {
    set_now_in_usec(NowInUsec() + advancement_usec);
  }

  // Advances the virtual 'now' by advancement_usec, and
  // calls WaitForEventAndExecteCallbacks.
  // Note that the WaitForEventsAndExecuteCallbacks invocation
  // may cause NowInUs to advance beyond what was specified here.
  // If that is not desired, use the AdvanceByExactly calls.
  void AdvanceByAndWaitForEventsAndExecuteCallbacks(int64_t advancement_usec) {
    AdvanceBy(advancement_usec);
    WaitForEventsAndExecuteCallbacks();
  }

 private:
  int64_t now_in_usec_;

  DISALLOW_COPY_AND_ASSIGN(FakeTimeEpollServer);
};

class MockEpollServer : public FakeTimeEpollServer {
 public:  // type definitions
  using EventQueue = std::unordered_multimap<int64_t, struct epoll_event>;

  MockEpollServer() : until_in_usec_(-1) {}
  ~MockEpollServer() override {}

  // time_in_usec is the time at which the event specified
  // by 'ee' will be delivered. Note that it -is- possible
  // to add an event for a time which has already been passed..
  // .. upon the next time that the callbacks are invoked,
  // all events which are in the 'past' will be delivered.
  void AddEvent(int64_t time_in_usec, const struct epoll_event &ee) {
    event_queue_.insert(std::make_pair(time_in_usec, ee));
  }

  // Advances the virtual 'now' by advancement_usec,
  // and ensure that the next invocation of
  // WaitForEventsAndExecuteCallbacks goes no farther than
  // advancement_usec from the current time.
  void AdvanceByExactly(int64_t advancement_usec) {
    until_in_usec_ = NowInUsec() + advancement_usec;
    set_now_in_usec(NowInUsec() + advancement_usec);
  }

  // As above, except calls WaitForEventsAndExecuteCallbacks.
  void AdvanceByExactlyAndCallCallbacks(int64_t advancement_usec) {
    AdvanceByExactly(advancement_usec);
    WaitForEventsAndExecuteCallbacks();
  }

  std::unordered_set<AlarmCB *>::size_type NumberOfAlarms() const {
    return all_alarms_.size();
  }

 protected:  // functions
  // These functions do nothing here, as we're not actually
  // using the epoll_* syscalls.
  void DelFD(int fd) const override {}
  void AddFD(int fd, int event_mask) const override {}
  void ModFD(int fd, int event_mask) const override {}

  // Replaces the epoll_server's epoll_wait_impl.
  int epoll_wait_impl(int epfd, struct epoll_event *events, int max_events,
                      int timeout_in_ms) override {
    int num_events = 0;
    while (!event_queue_.empty() && num_events < max_events &&
           event_queue_.begin()->first <= NowInUsec() &&
           ((until_in_usec_ == -1) ||
            (event_queue_.begin()->first < until_in_usec_))) {
      int64_t event_time_in_usec = event_queue_.begin()->first;
      events[num_events] = event_queue_.begin()->second;
      if (event_time_in_usec > NowInUsec()) {
        set_now_in_usec(event_time_in_usec);
      }
      event_queue_.erase(event_queue_.begin());
      ++num_events;
    }
    if (num_events == 0) {       // then we'd have waited 'till the timeout.
      if (until_in_usec_ < 0) {  // then we don't care what the final time is.
        if (timeout_in_ms > 0) {
          AdvanceBy(timeout_in_ms * 1000);
        }
      } else {  // except we assume that we don't wait for the timeout
        // period if until_in_usec_ is a positive number.
        set_now_in_usec(until_in_usec_);
        // And reset until_in_usec_ to signal no waiting (as
        // the AdvanceByExactly* stuff is meant to be one-shot,
        // as are all similar EpollServer functions)
        until_in_usec_ = -1;
      }
    }
    if (until_in_usec_ >= 0) {
      CHECK(until_in_usec_ >= NowInUsec());
    }
    return num_events;
  }
  void SetNonblocking(int fd) override {}

 private:  // members
  EventQueue event_queue_;
  int64_t until_in_usec_;

  DISALLOW_COPY_AND_ASSIGN(MockEpollServer);
};

TEST(EpollTimerTest, CreateTimer) {
  MockEpollServer epoll_server;
  std::unique_ptr<EpollTimer> timer(new EpollTimer(&epoll_server));
  timer->SetTimerCallback(
      std::bind(&TestTimerDelegate::OnTimer, &testTimerDelegate));

  Time start = epoll_server.Now();
  Duration delta = std::chrono::microseconds(1);
  timer->Set(start + delta);

  epoll_server.AdvanceByAndWaitForEventsAndExecuteCallbacks(delta.count());
  EXPECT_EQ(start + delta, epoll_server.Now());
}

TEST(EpollTimerTest, CreateTimerAndCancel) {
  MockEpollServer epoll_server;
  std::unique_ptr<EpollTimer> timer(new EpollTimer(&epoll_server));
  timer->SetTimerCallback(
      std::bind(&TestTimerDelegate::OnTimer, &testTimerDelegate));

  Time start = epoll_server.Now();
  Duration delta = std::chrono::microseconds(1);
  timer->Set(start + delta);
  timer->Cancel();

  epoll_server.AdvanceByExactlyAndCallCallbacks(delta.count());
  EXPECT_EQ(start + delta, epoll_server.Now());
  EXPECT_FALSE(testTimerDelegate.fired());
}

TEST(EpollTimerTest, CreateTimerAndReset) {
  MockEpollServer epoll_server;
  std::unique_ptr<EpollTimer> timer(new EpollTimer(&epoll_server));
  timer->SetTimerCallback(
      std::bind(&TestTimerDelegate::OnTimer, &testTimerDelegate));

  Time start = epoll_server.Now();
  Duration delta = std::chrono::microseconds(1);
  timer->Set(epoll_server.Now() + delta);
  timer->Cancel();
  Duration new_delta = std::chrono::microseconds(3);
  timer->Set(epoll_server.Now() + new_delta);

  epoll_server.AdvanceByExactlyAndCallCallbacks(delta.count());
  EXPECT_EQ((start + delta).ToMilliSeconds(),
            epoll_server.Now().ToMilliSeconds());
  EXPECT_FALSE(testTimerDelegate.fired());

  epoll_server.AdvanceByExactlyAndCallCallbacks((new_delta - delta).count());
  EXPECT_EQ((start + new_delta).ToMilliSeconds(),
            epoll_server.Now().ToMilliSeconds());
  EXPECT_TRUE(testTimerDelegate.fired());
}

TEST(EpollTimerTest, CreateTimerAndUpdate) {
  MockEpollServer epoll_server;
  std::unique_ptr<EpollTimer> timer(new EpollTimer(&epoll_server));
  timer->SetTimerCallback(
      std::bind(&TestTimerDelegate::OnTimer, &testTimerDelegate));

  Time start = epoll_server.Now();
  Duration delta = std::chrono::microseconds(1);
  timer->Set(epoll_server.Now() + delta);
  Duration new_delta = std::chrono::microseconds(3);
  timer->Update(epoll_server.Now() + new_delta);

  epoll_server.AdvanceByExactlyAndCallCallbacks(delta.count());
  EXPECT_EQ(start + delta, epoll_server.Now());
  EXPECT_FALSE(testTimerDelegate.fired());

  // Move the alarm forward 1us.
  timer->Update(epoll_server.Now() + new_delta);

  epoll_server.AdvanceByExactlyAndCallCallbacks((new_delta - delta).count());
  EXPECT_EQ(start + new_delta, epoll_server.Now());
  EXPECT_FALSE(testTimerDelegate.fired());

  // Set the alarm via an update call.
  new_delta = std::chrono::microseconds(5);
  timer->Update(epoll_server.Now() + new_delta);
  EXPECT_TRUE(timer->IsSet());

  // Update it with an uninitialized time and ensure it's cancelled.
  timer->Update(Time::UnixEpoch());
  EXPECT_FALSE(timer->IsSet());
}

}  // namespace
}  // namespace raner
