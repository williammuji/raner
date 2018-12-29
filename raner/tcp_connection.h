// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_TCP_CONNECTION_H_
#define RANER_NET_TCP_CONNECTION_H_

#include "raner/byte_buffer.h"
#include "raner/callbacks.h"
#include "raner/epoll_server.h"
#include "raner/epoll_timer.h"
#include "raner/socket.h"

#include <any>
#include <memory>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace raner {

class EventLoop;
class Socket;

class TCPConnection : public EpollCallbackInterface,
                      public std::enable_shared_from_this<TCPConnection> {
 public:
  TCPConnection(EventLoop* loop, const std::string& name,
                std::unique_ptr<Socket> socket);
  ~TCPConnection();

  EventLoop* GetLoop() const { return loop_; }
  bool Connected() const { return state_ == kConnected; }
  bool Disconnected() const { return state_ == kDisconnected; }

  // return true if success.
  bool GetTCPInfo(struct tcp_info*) const;
  std::string GetTCPInfoString() const;

  void Send(std::string&& message);  // C++11
  void Send(const void* message, int len);
  void Send(std::string_view message);
  void Send(ByteBuffer* message);  // this one will swap data
  void Shutdown();                 // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no
  // simultaneous calling
  void ForceClose();
  void ForceCloseWithDelay(double seconds);
  void SetTCPNoDelay();
  // reading or not
  void StartRead();
  void StopRead();
  bool IsReading() const {
    return reading_;
  };  // NOT thread safe, may race with start/stopReadInLoop

  void SetContext(const std::any& context) { context_ = context; }

  const std::any& GetContext() const { return context_; }

  std::any* GetMutableContext() { return &context_; }

  void SetConnectionCallback(const ConnectionCallback& cb) {
    connection_callback_ = cb;
  }

  void SetMessageCallback(const MessageCallback& cb) { message_callback_ = cb; }

  void SetWriteCompleteCallback(const WriteCompleteCallback& cb) {
    write_complete_callback_ = cb;
  }

  void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                size_t high_water_mark) {
    high_water_mark_callback_ = cb;
    high_water_mark_ = high_water_mark;
  }

  /// Advanced interface
  ByteBuffer* input_buffer() { return &input_buffer_; }

  ByteBuffer* output_buffer() { return &output_buffer_; }

  /// Internal use only.
  void SetCloseCallback(const CloseCallback& cb) { close_callback_ = cb; }

  // called when TCPServer accepts a new connection
  void ConnectEstablished();  // should be called only once
  // called when TCPServer has removed me from its map
  void ConnectDestroyed();  // should be called only once

  std::string GetLocalAddr() const { return socket_->GetLocalAddr(); }
  std::string GetPeerAddr() const { return socket_->GetPeerAddr(); }

  // From EpollCallbackInterface
  void OnRegistration(EpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, EpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}
  void OnShutdown(EpollServer* eps, int fd) override {}
  std::string Name() const override { return name_; }

 private:
  enum State { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead();
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(std::string&& message);
  void sendInLoop(std::string_view message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  void setState(State s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  EventLoop* loop_;
  const std::string name_;
  State state_;  // FIXME: use atomic variable
  bool reading_;
  // we don't expose those classes to client.
  std::unique_ptr<Socket> socket_;
  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
  WriteCompleteCallback write_complete_callback_;
  HighWaterMarkCallback high_water_mark_callback_;
  CloseCallback close_callback_;
  size_t high_water_mark_;
  ByteBuffer input_buffer_;
  ByteBuffer output_buffer_;  // FIXME: use list<ByteBuffer> as output buffer.
  std::any context_;

  std::unique_ptr<EpollTimer> force_close_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnection);
};

typedef std::shared_ptr<TCPConnection> TCPConnectionPtr;

}  // namespace raner

#endif  // RANER_NET_TCP_CONNECTION_H_
