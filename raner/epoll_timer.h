// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_EPOLL_TIMER_H_
#define RANER_NET_EPOLL_TIMER_H_

#include <memory>
#include "raner/callbacks.h"
#include "raner/epoll_server.h"
#include "raner/macros.h"
#include "raner/time.h"

namespace raner {

class EpollServer;

// Represent a epoll timer which will go off at a scheduled time,
// and execute the |OnTimer| method of the delegate.
// A timer may be cancelled, in which case it may or may not be
// removed from the underlying scheduling system, but in either case
// the task will not be executed.
class EpollTimer {
 public:
  EpollTimer(EpollServer *epoll_server);

  // Complex class/struct needs an explicit out-of-line destructor
  // https://www.chromium.org/developers/coding-style/chromium-style-checker-errors
  ~EpollTimer();

  // Sets the timer to fire at |deadline|.  Must not be called while
  // the timer is set.  To reschedule a timer, call Cancel() first,
  // then Set().
  void Set(Time new_deadline);

  // Cancels the timer.  May be called repeatedly.  Does not
  // guarantee that the underlying scheduling system will remove
  // the timer's associated task, but guarantees that the
  // delegates OnTimer method will not be called.
  void Cancel();

  // Cancels and sets the timer if the |deadline| is farther from the current
  // deadline than |granularity|, and otherwise does nothing.  If |deadline| is
  // not initialized, the timer is cancelled.
  void Update(Time new_deadline);

  // Returns true if |deadline_| has been set to a non-zero time.
  bool IsSet() const;

  Time deadline() const { return deadline_; }

  void SetTimerCallback(TimerCallback timer_cb) {
    timer_cb_ = std::move(timer_cb);
  }

 private:
  // Implement this method to perform the scheduling of the timer.
  // Is called from Set() or Fire(), after the deadline has been updated.
  void setImpl();

  // Implement this method to perform the cancelation of the timer.
  void cancelImpl();

  // Implement this method to perform the update of the timer if there
  // exists a more optimal implementation than calling CancelImpl() and
  // SetImpl().
  void updateImpl();

  // Called when the timer fires.  Invokes the delegates |OnTimer|
  // if a delegate is set, and if the deadline has been exceeded.
  // Implementations which do not remove the timer from the underlying
  // scheduler on Cancel() may need to handle the situation where the
  // task executes before the deadline has been reached, in which case they
  // need to reschedule the task and must notcall invoke this method.
  void fire();

  class EpollTimerImpl : public EpollAlarm {
   public:
    explicit EpollTimerImpl(EpollTimer *timer) : timer_(timer) {}

    int64_t OnAlarm() override;

   private:
    EpollTimer *timer_;
  };

  EpollServer *epoll_server_;
  EpollTimerImpl epoll_timer_impl_;
  TimerCallback timer_cb_;
  Time deadline_;

  DISALLOW_COPY_AND_ASSIGN(EpollTimer);
};

}  // namespace raner

#endif  // RANER_NET_EPOLL_TIMER_H_
