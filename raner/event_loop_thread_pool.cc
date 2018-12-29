// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/event_loop_thread_pool.h"

#include "raner/event_loop.h"
#include "raner/event_loop_thread.h"

#include <assert.h>
#include <stdio.h>

namespace raner {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop,
                                         std::string_view name_arg)
    : base_loop_(base_loop),
      name_(name_arg),
      started_(false),
      num_threads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::Start(const ThreadInitCallback& cb) {
  assert(!started_);
  base_loop_->AssertInLoopThread();

  started_ = true;

  for (int i = 0; i < num_threads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
    EventLoopThread* t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    loops_.push_back(t->StartLoop());
  }
  if (num_threads_ == 0 && cb) {
    cb(base_loop_);
  }
}

EventLoop* EventLoopThreadPool::GetNextLoop() {
  base_loop_->AssertInLoopThread();
  assert(started_);
  EventLoop* loop = base_loop_;

  if (!loops_.empty()) {
    // round-robin
    loop = loops_[next_];
    ++next_;
    if (static_cast<size_t>(next_) >= loops_.size()) {
      next_ = 0;
    }
  }
  return loop;
}

EventLoop* EventLoopThreadPool::GetLoopForHash(size_t hashCode) {
  base_loop_->AssertInLoopThread();
  EventLoop* loop = base_loop_;

  if (!loops_.empty()) {
    loop = loops_[hashCode % loops_.size()];
  }
  return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::GetAllLoops() {
  base_loop_->AssertInLoopThread();
  assert(started_);
  if (loops_.empty()) {
    return std::vector<EventLoop*>(1, base_loop_);
  } else {
    return loops_;
  }
}

}  // namespace raner
