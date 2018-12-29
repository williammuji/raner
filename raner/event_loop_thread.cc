// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/event_loop_thread.h"

#include "raner/event_loop.h"

namespace raner {

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(NULL), exiting_(false), callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != NULL)  // not 100% race-free, eg. run could be running callback_.
  {
    // still a tiny chance to call destructed object, if run exits just
    // now. but when EventLoopThread destructs, usually programming is exiting
    // anyway.
    loop_->QueueInLoop(std::bind(&EventLoop::Quit, loop_));
    // loop_->Quit();
    thread_->join();
  }
}

EventLoop* EventLoopThread::StartLoop() {
  thread_.reset(new std::thread(std::bind(&EventLoopThread::run, this)));

  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == NULL) {
      condition_variable_.wait(lock);
    }
  }

  return loop_;
}

void EventLoopThread::run() {
  EventLoop loop;

  if (callback_) {
    callback_(&loop);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = &loop;
    condition_variable_.notify_one();
  }

  loop.Loop();
  // assert(exiting_);
  loop_ = NULL;
}

}  // namespace raner
