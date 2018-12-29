// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_EVENT_LOOP_H_
#define RANER_NET_EVENT_LOOP_H_

#include <any>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include <mutex>
#include "raner/callbacks.h"
#include "raner/epoll_server.h"
#include "raner/epoll_timer.h"

namespace raner {

class EpollServer;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
class EventLoop {
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void Loop();

  /// Quits loop.
  ///
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<EventLoop> for 100% safety.
  void Quit();

  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void RunInLoop(Functor cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void QueueInLoop(Functor cb);

  size_t QueueSize() const;

  // pid_t threadId() const { return thread_id_; }
  void AssertInLoopThread() {
    if (!IsInLoopThread()) {
      abortNotInLoopThread();
    }
  }
  bool IsInLoopThread() const {
    return thread_id_ == std::this_thread::get_id();
  }

  void SetContext(const std::any &context) { context_ = context; }

  const std::any &GetContext() const { return context_; }

  std::any *GetMutableContext() { return &context_; }

  EpollServer *epoll_server() { return &epoll_server_; }

  std::unique_ptr<EpollTimer> CreateTimer(TimerCallback timer_cb);

  static EventLoop *GetEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void doPendingFunctors();

  bool looping_; /* atomic */
  std::atomic<bool> quit_;
  bool calling_pending_functors_;
  bool callingPendingFunctors_; /* atomic */
  int64_t iteration_;
  const std::thread::id thread_id_;
  EpollServer epoll_server_;
  std::any context_;

  mutable std::mutex mutex_;
  std::vector<Functor> pending_functors_;  // @GuardedBy mutex_

  DISALLOW_COPY_AND_ASSIGN(EventLoop);
};

}  // namespace raner

#endif  // RANER_NET_EVENT_LOOP_H_
