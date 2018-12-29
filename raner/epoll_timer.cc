// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/epoll_timer.h"

#include <glog/logging.h>

namespace raner {

int64_t EpollTimer::EpollTimerImpl::OnAlarm() {
  EpollAlarm::OnAlarm();
  timer_->fire();
  // Fire will take care of registering the timer, if needed.
  return 0;
}

EpollTimer::EpollTimer(EpollServer *epoll_server)
    : epoll_server_(epoll_server),
      epoll_timer_impl_(this),
      deadline_(Time::UnixEpoch()) {}

EpollTimer::~EpollTimer() {}

void EpollTimer::Set(Time new_deadline) {
  DCHECK(!IsSet());
  DCHECK(new_deadline.IsInitialized());
  deadline_ = new_deadline;
  setImpl();
}

void EpollTimer::Cancel() {
  if (!IsSet()) {
    // Don't try to cancel an timer that hasn't been set.
    return;
  }
  deadline_ = Time::UnixEpoch();
  cancelImpl();
}

void EpollTimer::Update(Time new_deadline) {
  if (!new_deadline.IsInitialized()) {
    Cancel();
    return;
  }

  const bool was_set = IsSet();
  deadline_ = new_deadline;
  if (was_set) {
    updateImpl();
  } else {
    setImpl();
  }
}

bool EpollTimer::IsSet() const { return deadline_.IsInitialized(); }

void EpollTimer::fire() {
  if (!IsSet()) {
    return;
  }

  deadline_ = Time::UnixEpoch();
  if (timer_cb_) timer_cb_();
}

void EpollTimer::updateImpl() {
  // cancelImpl and setImpl take the new deadline by way of the deadline_
  // member, so save and restore deadline_ before canceling.
  const Time new_deadline = deadline_;

  deadline_ = Time::UnixEpoch();
  cancelImpl();

  deadline_ = new_deadline;
  setImpl();
}

void EpollTimer::setImpl() {
  DCHECK(deadline().IsInitialized());
  epoll_server_->RegisterAlarm((deadline() - Time::UnixEpoch()).count(),
                               &epoll_timer_impl_);
}

void EpollTimer::cancelImpl() {
  DCHECK(!deadline().IsInitialized());
  epoll_timer_impl_.UnregisterIfRegistered();
}

}  // namespace raner
