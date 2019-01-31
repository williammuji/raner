// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/tcp_client.h"

#include <glog/logging.h>
#include "raner/event_loop.h"
#include "raner/safe_strerror.h"
#include "raner/socket.h"

#include <stdio.h>  // snprintf
#include <functional>

namespace {
const int kEpollFlags = EPOLLOUT;
}  // namespace

namespace raner {
namespace detail {

void removeConnection(EventLoop *loop, const TCPConnectionPtr &conn) {
  loop->QueueInLoop(std::bind(&TCPConnection::ConnectDestroyed, conn));
}

}  // namespace detail

TCPClient::TCPClient(EventLoop *loop, std::string_view host, int port,
                     std::string_view name)
    : loop_(CHECK_NOTNULL(loop)),
      host_(host),
      port_(port),
      name_(name),
      connect_(false),
      state_(kDisconnected),
      retry_interval_ms_(kInitRetryIntervalMs),
      connection_callback_(defaultConnectionCallback),
      message_callback_(defaultMessageCallback),
      retry_(false),
      next_conn_id_(1) {
  LOG(INFO) << "TCPClient::TCPClient[" << name_ << "] " << this;
}

TCPClient::~TCPClient() {
  LOG(INFO) << "TCPClient::~TCPClient[" << name_ << "] " << this;
  TCPConnectionPtr conn;
  bool unique = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    unique = connection_.unique();
    conn = connection_;
  }
  if (conn) {
    assert(loop_ == conn->GetLoop());
    // FIXME: not 100% safe, if we are in different thread
    CloseCallback cb = std::bind(&detail::removeConnection, loop_, _1);
    loop_->RunInLoop(std::bind(&TCPConnection::SetCloseCallback, conn, cb));
    if (unique) {
      conn->ForceClose();
    }
  } else {
    if (state_ == kConnecting) {
      setState(kDisconnected);
      loop_->epoll_server()->UnregisterFD(socket_->fd());
    }
  }
}

void TCPClient::Connect() {
  // FIXME: check state
  LOG(INFO) << "TCPClient::connect[" << name_ << "] - connecting to " << host_
            << ":" << port_;
  connect_ = true;
  loop_->RunInLoop(std::bind(&TCPClient::startInLoop, this));  // FIXME: unsafe
}

void TCPClient::Stop() {
  connect_ = false;
  loop_->QueueInLoop(std::bind(&TCPClient::stopInLoop, shared_from_this()));
}

void TCPClient::Disconnect() {
  connect_ = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
      connection_->Shutdown();
    }
  }
}

void TCPClient::startInLoop() {
  loop_->AssertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_) {
    connect();
  } else {
    LOG(INFO) << "do not connect";
  }
}

void TCPClient::restart() {
  loop_->AssertInLoopThread();
  setState(kDisconnected);
  retry_interval_ms_ = kInitRetryIntervalMs;
  connect_ = true;
  startInLoop();
}

void TCPClient::stopInLoop() {
  loop_->AssertInLoopThread();
  if (state_ == kConnecting) {
    setState(kDisconnected);
    loop_->epoll_server()->UnregisterFD(socket_->fd());
    retry();
  }
}

void TCPClient::connect() {
  socket_.reset(new Socket());
  if (!socket_->Connect(host_, port_)) {
    LOG(ERROR) << "connect error " << safe_strerror(errno);
    retry();
  } else {
    connecting();
  }
}

void TCPClient::connecting() {
  setState(kConnecting);
  loop_->epoll_server()->RegisterFD(socket_->fd(), this, kEpollFlags);
}

void TCPClient::retry() {
  socket_->Close();
  setState(kDisconnected);
  if (connect_) {
    LOG(INFO) << "retry - Retry connecting to " << host_ << ":" << port_
              << " in " << retry_interval_ms_ << " milliseconds. ";

    if (!reconnect_timer_) {
      reconnect_timer_ =
          loop_->CreateTimer(std::bind(&TCPClient::Connect, this));
    }
    reconnect_timer_->Update(Time::Now() + Duration(retry_interval_ms_ * 1000));
    retry_interval_ms_ = std::min(retry_interval_ms_ * 2, kMaxRetryIntervalMs);
  } else {
    LOG(INFO) << "do not connect";
  }
}

void TCPClient::newConnection() {
  loop_->AssertInLoopThread();

  char buf[32];
  snprintf(buf, sizeof(buf), ":%s:%u#%d", host_.c_str(), port_, next_conn_id_);
  ++next_conn_id_;
  std::string conn_name = name_ + buf;

  TCPConnectionPtr conn(
      new TCPConnection(loop_, conn_name, std::move(socket_)));

  conn->SetConnectionCallback(connection_callback_);
  conn->SetMessageCallback(message_callback_);
  conn->SetWriteCompleteCallback(write_complete_callback_);
  conn->SetCloseCallback(
      std::bind(&TCPClient::removeConnection, this, _1));  // FIXME: unsafe
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_ = conn;
  }
  conn->ConnectEstablished();
}

void TCPClient::removeConnection(const TCPConnectionPtr &conn) {
  loop_->AssertInLoopThread();
  assert(loop_ == conn->GetLoop());

  {
    std::lock_guard<std::mutex> lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();
  }

  LOG(INFO) << "TCPClient::removeConnection " << name_;

  loop_->QueueInLoop(std::bind(&TCPConnection::ConnectDestroyed, conn));
  if (retry_ && connect_) {
    LOG(INFO) << "TCPClient::connect[" << name_ << "] - Reconnecting to "
              << host_ << ":" << port_;
    restart();
  }
}

void TCPClient::handleWrite() {
  LOG(INFO) << "TCPClient::handleWrite " << state_;

  if (state_ == kConnecting) {
    loop_->epoll_server()->UnregisterFD(socket_->fd());
    int err = socket_->GetSocketError();
    if (err) {
      LOG(WARNING) << "TCPClient::handleWrite - SO_ERROR = " << err << " "
                   << safe_strerror(err);
      retry();
    } else {
      setState(kConnected);
      if (connect_) {
        newConnection();
      } else {
        socket_->Close();
      }
    }
  } else {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void TCPClient::handleError() {
  LOG(ERROR) << "TCPClient::handleError state=" << state_;
  if (state_ == kConnecting) {
    loop_->epoll_server()->UnregisterFD(socket_->fd());
    int err = socket_->GetSocketError();
    if (err) {
      LOG(WARNING) << "TCPClient::handleError - SO_ERROR = " << err << " "
                   << safe_strerror(err);
      retry();
    }
  }
}

void TCPClient::OnEvent(int fd, EpollEvent *event) {
  LOG(INFO) << "OnEvent " << fd;

  if (event->in_events & EPOLLOUT) {
    handleWrite();
  }
  if (event->in_events & EPOLLERR) {
    handleError();
  }
}

}  // namespace raner
