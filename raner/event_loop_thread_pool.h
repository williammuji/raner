// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_EVENT_LOOP_THREAD_POOL_H_
#define RANER_NET_EVENT_LOOP_THREAD_POOL_H_

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "raner/macros.h"

namespace raner {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
 public:
  typedef std::function<void(EventLoop *)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop *base_loop, std::string_view name_arg);
  ~EventLoopThreadPool();

  void SetThreadNum(int num_threads) { num_threads_ = num_threads; }
  void Start(const ThreadInitCallback &cb = ThreadInitCallback());

  // valid after calling start()
  /// round-robin
  EventLoop *GetNextLoop();

  /// with the same hash code, it will always return the same EventLoop
  EventLoop *GetLoopForHash(size_t hash_code);

  std::vector<EventLoop *> GetAllLoops();

  bool Started() const { return started_; }

  const std::string &Name() const { return name_; }

 private:
  EventLoop *base_loop_;
  std::string name_;
  bool started_;
  int num_threads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop *> loops_;

  DISALLOW_COPY_AND_ASSIGN(EventLoopThreadPool);
};

}  // namespace raner

#endif  // RANER_NET_EVENT_LOOP_THREAD_POOL_H_
