// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_TCP_CLIENT_H_
#define RANER_NET_TCP_CLIENT_H_

#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include "raner/epoll_server.h"
#include "raner/tcp_connection.h"

namespace raner {

class EventLoop;

class TCPClient : public std::enable_shared_from_this<TCPClient>,  // FIXME
                  public EpollCallbackInterface {
 public:
  typedef std::function<void(int sock_fd)> NewConnectionCallback;

  TCPClient(EventLoop* loop, std::string_view host, int port,
            std::string_view name);
  ~TCPClient();  // force out-line dtor, for std::unique_ptr members.

  void Connect();
  void Disconnect();
  void Stop();

  TCPConnectionPtr Connection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_;
  }

  EventLoop* GetLoop() const { return loop_; }
  bool retry() const { return retry_; }
  void EnableRetry() { retry_ = true; }

  const std::string host() const { return host_; }
  int port() const { return port_; }

  /// Set connection callback.
  /// Not thread safe.
  void SetConnectionCallback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
  }

  /// Set message callback.
  /// Not thread safe.
  void SetMessageCallback(MessageCallback cb) {
    message_callback_ = std::move(cb);
  }

  /// Set write complete callback.
  /// Not thread safe.
  void SetWriteCompleteCallback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
  }

  // From EpollCallbackInterface
  void OnRegistration(EpollServer* eps, int fd, int event_mask) override {
  }  // FIXME
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, EpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}
  void OnShutdown(EpollServer* eps, int fd) override {}
  std::string Name() const override { return name_; }

 private:
  enum State { kDisconnected, kConnecting, kConnected };
  static constexpr int kMaxRetryIntervalMs = 30 * 1000;
  static constexpr int kInitRetryIntervalMs = 500;

  /// Not thread safe, but in loop
  void newConnection();
  /// Not thread safe, but in loop
  void removeConnection(const TCPConnectionPtr& conn);

  void setState(State s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void restart();
  void connect();
  void connecting();
  void handleWrite();
  void handleError();
  void retry();

  EventLoop* loop_;  // not owned

  const std::string host_;
  int port_;
  const std::string name_;
  bool connect_;  // atomic
  State state_;
  int retry_interval_ms_;

  std::unique_ptr<Socket> socket_;
  std::unique_ptr<EpollTimer> reconnect_timer_;

  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
  WriteCompleteCallback write_complete_callback_;
  bool retry_;  // atomic
  // always in loop thread
  int next_conn_id_;
  mutable std::mutex mutex_;
  TCPConnectionPtr connection_;  // @GuardedBy mutex_

  DISALLOW_COPY_AND_ASSIGN(TCPClient);
};

}  // namespace raner

#endif  // RANER_NET_TCP_CLIENT_H_
