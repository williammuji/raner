// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_EVENT_LOOP_THREAD_H_
#define RANER_NET_EVENT_LOOP_THREAD_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "raner/macros.h"

namespace raner {

class EventLoop;

class EventLoopThread {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const std::string& name = std::string());
  ~EventLoopThread();

  EventLoop* StartLoop();

 private:
  void run();

  EventLoop* loop_;
  bool exiting_;
  std::unique_ptr<std::thread> thread_;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  ThreadInitCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EventLoopThread);
};

}  // namespace raner

#endif  // RANER_NET_EVENT_LOOP_THREAD_H_
