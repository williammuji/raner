// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_TCP_SERVER_H_
#define RANER_NET_TCP_SERVER_H_

#include "raner/epoll_server.h"
#include "raner/tcp_connection.h"

#include <atomic>
#include <map>

namespace raner {

class Socket;
class EventLoop;
class EventLoopThreadPool;

class TCPServer : public EpollCallbackInterface {
 public:
  typedef std::function<void(EventLoop *)> ThreadInitCallback;

  TCPServer(EventLoop *loop, std::string_view host, int port,
            std::string_view name);
  ~TCPServer();  // force out-line dtor, for std::unique_ptr members.

  const std::string host() const { return host_; }
  int port() const { return port_; }
  EventLoop *GetLoop() const { return loop_; }

  /// Set the number of threads for handling input.
  ///
  /// Always accepts new connection in loop's thread.
  /// Must be called before @c start
  /// @param numThreads
  /// - 0 means all I/O in loop's thread, no thread will created.
  ///   this is the default value.
  /// - 1 means all I/O in another thread.
  /// - N means a thread pool with N threads, new connections
  ///   are assigned on a round-robin basis.
  void SetThreadNum(int num_threads);
  void SetThreadInitCallback(const ThreadInitCallback &cb) {
    thread_init_callback_ = cb;
  }
  /// valid after calling start()
  std::shared_ptr<EventLoopThreadPool> thread_pool() { return thread_pool_; }

  /// Starts the server if it's not listenning.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void Start();

  /// Set connection callback.
  /// Not thread safe.
  void SetConnectionCallback(const ConnectionCallback &cb) {
    connection_callback_ = cb;
  }

  /// Set message callback.
  /// Not thread safe.
  void SetMessageCallback(const MessageCallback &cb) { message_callback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void SetWriteCompleteCallback(const WriteCompleteCallback &cb) {
    write_complete_callback_ = cb;
  }

  // From EpollCallbackInterface
  void OnRegistration(EpollServer *eps, int fd, int event_mask) override {
  }  // FIXME
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, EpollEvent *event) override;
  void OnUnregistration(int fd, bool replaced) override {}
  void OnShutdown(EpollServer *eps, int fd) override {}
  std::string Name() const override { return name_; }

 private:
  void createSocketAndListen();
  void handleRead();

  /// Not thread safe, but in loop
  void newConnection(std::unique_ptr<Socket> client_socket);
  /// Thread safe.
  void removeConnection(const TCPConnectionPtr &conn);
  /// Not thread safe, but in loop
  void removeConnectionInLoop(const TCPConnectionPtr &conn);

  typedef std::map<std::string, TCPConnectionPtr> ConnectionMap;

  EventLoop *loop_;
  const std::string host_;
  const int port_;
  const std::string name_;

  std::unique_ptr<Socket> socket_;  // avoid revealing Socket
  bool listenning_;
  int idle_fd_;
  std::shared_ptr<EventLoopThreadPool> thread_pool_;

  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
  WriteCompleteCallback write_complete_callback_;
  ThreadInitCallback thread_init_callback_;

  std::atomic_int32_t started_;
  // always in loop thread
  int next_conn_id_;
  ConnectionMap connections_;

  DISALLOW_COPY_AND_ASSIGN(TCPServer);
};

}  // namespace raner

#endif  //  RANER_NET_TCP_SERVER_H_
