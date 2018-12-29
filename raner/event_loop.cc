// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/event_loop.h"

#include <glog/logging.h>
#include "raner/socket.h"

#include <assert.h>
#include <signal.h>
#include <unistd.h>

namespace {
__thread raner::EventLoop *t_loopInThisThread = nullptr;
const int kPollTimeMs = 10000;

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe {
 public:
  IgnoreSigPipe() {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG(INFO) << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

namespace raner {

// static
EventLoop *EventLoop::GetEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      calling_pending_functors_(false),
      iteration_(0),
      thread_id_(std::this_thread::get_id()) {
  LOG(INFO) << "EventLoop created " << this << " in thread " << thread_id_;
  if (t_loopInThisThread) {
    LOG(FATAL) << "Another EventLoop " << t_loopInThisThread
               << " exists in this thread " << thread_id_;
  } else {
    t_loopInThisThread = this;
  }

  epoll_server_.set_timeout_in_us(kPollTimeMs);
}

EventLoop::~EventLoop() {
  LOG(INFO) << "EventLoop " << this << " of thread " << thread_id_
            << " destructs in thread " << std::this_thread::get_id();
  t_loopInThisThread = nullptr;
}

void EventLoop::Loop() {
  assert(!looping_);
  AssertInLoopThread();
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG(INFO) << "EventLoop " << this << " start looping";

  while (!quit_) {
    epoll_server_.WaitForEventsAndExecuteCallbacks();
    ++iteration_;
    doPendingFunctors();
  }

  LOG(INFO) << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::Quit() {
  quit_ = true;
  // There is a chance that loop() just executes while(!quit_) and exits,
  // then EventLoop destructs, then we are accessing an invalid object.
  // Can be fixed using mutex_ in both places.
  if (!IsInLoopThread()) {
    epoll_server_.Wake();
  }
}

void EventLoop::RunInLoop(Functor cb) {
  if (IsInLoopThread()) {
    cb();
  } else {
    QueueInLoop(std::move(cb));
  }
}

void EventLoop::QueueInLoop(Functor cb) {
  {
    std::lock_guard<std::mutex> guard(mutex_);
    pending_functors_.push_back(std::move(cb));
  }

  if (!IsInLoopThread() || calling_pending_functors_) {
    epoll_server_.Wake();
  }
}

size_t EventLoop::QueueSize() const {
  std::lock_guard<std::mutex> guard(mutex_);
  return pending_functors_.size();
}

void EventLoop::abortNotInLoopThread() {
  LOG(FATAL) << "EventLoop::abortNotInLoopThread - EventLoop " << this
             << " was created in thread_id_ = " << thread_id_
             << ", current thread id = " << std::this_thread::get_id();
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  calling_pending_functors_ = true;

  {
    std::lock_guard<std::mutex> guard(mutex_);
    functors.swap(pending_functors_);
  }

  for (size_t i = 0; i < functors.size(); ++i) {
    functors[i]();
  }
  calling_pending_functors_ = false;
}

std::unique_ptr<EpollTimer> EventLoop::CreateTimer(TimerCallback timer_cb) {
  std::unique_ptr<EpollTimer> epoll_timer(new EpollTimer(&epoll_server_));
  epoll_timer->SetTimerCallback(std::move(timer_cb));
  return epoll_timer;
}

}  // namespace raner
