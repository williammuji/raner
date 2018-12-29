// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/tcp_connection.h"

#include <glog/logging.h>
#include "raner/event_loop.h"
#include "raner/safe_strerror.h"
#include "raner/socket.h"

#include <errno.h>

namespace {
const int kEpollFlags = EPOLLIN;
}  // namespace

namespace raner {

void defaultConnectionCallback(const TCPConnectionPtr &conn) {
  LOG(INFO) << conn->GetLocalAddr() << " -> " << conn->GetPeerAddr() << " is "
            << (conn->Connected() ? "UP" : "DOWN");
  // do not call conn->ForceClose(), because some users want to register message
  // callback only.
}

void defaultMessageCallback(const TCPConnectionPtr &, ByteBuffer *buf) {
  buf->SkipAll();
}

TCPConnection::TCPConnection(EventLoop *loop, const std::string &name,
                             std::unique_ptr<Socket> socket)
    : loop_(CHECK_NOTNULL(loop)),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(std::move(socket)),
      high_water_mark_(64 * 1024 * 1024) {
  LOG(INFO) << "TCPConnection::ctor[" << name_ << "] at " << this
            << " fd=" << socket_->fd();
  socket_->SetKeepAlive(true);
}

TCPConnection::~TCPConnection() {
  LOG(INFO) << "TCPConnection::dtor[" << name_ << "] at " << this
            << " fd=" << socket_->fd() << " state=" << stateToString();
  assert(state_ == kDisconnected);
}

bool TCPConnection::GetTCPInfo(struct tcp_info *tcpi) const {
  return socket_->GetTCPInfo(tcpi);
}

std::string TCPConnection::GetTCPInfoString() const {
  char buf[1024];
  buf[0] = '\0';
  socket_->GetTCPInfoString(buf, sizeof(buf));
  return buf;
}

void TCPConnection::Send(std::string &&message) {
  Send(message.data(), static_cast<int>(message.size()));
}

void TCPConnection::Send(const void *data, int len) {
  Send(std::string_view(static_cast<const char *>(data), len));
}

void TCPConnection::Send(std::string_view message) {
  if (state_ == kConnected) {
    if (loop_->IsInLoopThread()) {
      sendInLoop(message);
    } else {
      void (TCPConnection::*fp)(std::string_view message) =
          &TCPConnection::sendInLoop;
      loop_->RunInLoop(std::bind(fp,
                                 this,  // FIXME
                                 std::string(message)));
      // std::forward<string>(message)));
    }
  }
}

// FIXME efficiency!!!
void TCPConnection::Send(ByteBuffer *buf) {
  if (state_ == kConnected) {
    if (loop_->IsInLoopThread()) {
      sendInLoop(buf->BeginRead(), buf->ReadableBytes());
      buf->SkipAll();
    } else {
      void (TCPConnection::*fp)(std::string_view message) =
          &TCPConnection::sendInLoop;
      loop_->RunInLoop(std::bind(fp,
                                 this,  // FIXME
                                 buf->SkipAllAsString()));
      // std::forward<std::string>(message)));
    }
  }
}

void TCPConnection::sendInLoop(std::string_view message) {
  sendInLoop(message.data(), message.size());
}

void TCPConnection::sendInLoop(const void *data, size_t len) {
  loop_->AssertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool fault_error = false;
  if (state_ == kDisconnected) {
    LOG(WARNING) << "disConnected, give up writing";
    return;
  }
  // if no thinoutput_buffer_g in output queue, try writing directly
  if (!loop_->epoll_server()->HasRegisterWrite(socket_->fd()) &&
      output_buffer_.ReadableBytes() == 0) {
    nwrote = socket_->Write(data, len);
    if (nwrote >= 0) {
      remaining = len - nwrote;
      if (remaining == 0 && write_complete_callback_) {
        loop_->QueueInLoop(
            std::bind(write_complete_callback_, shared_from_this()));
      }
    } else {  // nwrote < 0
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG(ERROR) << "TCPConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET)  // FIXME: any others?
        {
          fault_error = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!fault_error && remaining > 0) {
    size_t old_len = output_buffer_.ReadableBytes();
    if (old_len + remaining >= high_water_mark_ && old_len < high_water_mark_ &&
        high_water_mark_callback_) {
      loop_->QueueInLoop(std::bind(high_water_mark_callback_,
                                   shared_from_this(), old_len + remaining));
    }
    output_buffer_.Write(static_cast<const char *>(data) + nwrote, remaining);
    if (!loop_->epoll_server()->HasRegisterWrite(socket_->fd())) {
      loop_->epoll_server()->StartWrite(socket_->fd());
    }
  }
}

void TCPConnection::Shutdown() {
  // FIXME: use compare and swap
  if (state_ == kConnected) {
    setState(kDisconnecting);
    // FIXME: shared_from_this()?
    loop_->RunInLoop(std::bind(&TCPConnection::shutdownInLoop, this));
  }
}

void TCPConnection::shutdownInLoop() {
  loop_->AssertInLoopThread();
  if (!loop_->epoll_server()->HasRegisterWrite(socket_->fd())) {
    // we are not writing
    socket_->ShutdownWrite();
  }
}

void TCPConnection::ForceClose() {
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting) {
    setState(kDisconnecting);
    loop_->QueueInLoop(
        std::bind(&TCPConnection::forceCloseInLoop, shared_from_this()));
  }
}

void TCPConnection::ForceCloseWithDelay(double seconds) {
  if (state_ == kConnected || state_ == kDisconnecting) {
    setState(kDisconnecting);
    force_close_delay_timer_ =
        loop_->CreateTimer(std::bind(&TCPConnection::ForceClose, this));
    force_close_delay_timer_->Update(
        Time::Now() +
        Duration(static_cast<int64_t>(
            seconds * 1000 *
            1000)));  // not forceCloseInLoop to avoid race condition
  }
}

void TCPConnection::forceCloseInLoop() {
  loop_->AssertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting) {
    // as if we received 0 byte in handleRead();
    handleClose();
  }
}

const char *TCPConnection::stateToString() const {
  switch (state_) {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TCPConnection::SetTCPNoDelay() { socket_->SetTCPNoDelay(); }

void TCPConnection::StartRead() {
  loop_->RunInLoop(std::bind(&TCPConnection::startReadInLoop, this));
}

void TCPConnection::startReadInLoop() {
  loop_->AssertInLoopThread();
  if (!reading_ || !loop_->epoll_server()->HasRegisterRead(socket_->fd())) {
    loop_->epoll_server()->StartRead(socket_->fd());
    reading_ = true;
  }
}

void TCPConnection::StopRead() {
  loop_->RunInLoop(std::bind(&TCPConnection::stopReadInLoop, this));
}

void TCPConnection::stopReadInLoop() {
  loop_->AssertInLoopThread();
  if (reading_ || loop_->epoll_server()->HasRegisterRead(socket_->fd())) {
    loop_->epoll_server()->StopRead(socket_->fd());
    reading_ = false;
  }
}

void TCPConnection::ConnectEstablished() {
  loop_->AssertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  loop_->epoll_server()->RegisterFD(socket_->fd(), this, kEpollFlags);

  connection_callback_(shared_from_this());
}

void TCPConnection::ConnectDestroyed() {
  loop_->AssertInLoopThread();
  if (state_ == kConnected) {
    setState(kDisconnected);
    loop_->epoll_server()->UnregisterFD(socket_->fd());

    connection_callback_(shared_from_this());
  }
}

void TCPConnection::OnEvent(int fd, EpollEvent *event) {
  loop_->AssertInLoopThread();

  event->out_ready_mask = 0;

  if (event->in_events & EPOLLIN) {
    handleRead();
  }
  if (event->in_events & EPOLLOUT) {
    handleWrite();
  }
  if (event->in_events & EPOLLERR) {
    handleError();
  }
}

void TCPConnection::handleRead() {
  loop_->AssertInLoopThread();
  int saved_errno = 0;
  ssize_t n = input_buffer_.ReadFD(socket_->fd(), &saved_errno);
  if (n > 0) {
    message_callback_(shared_from_this(), &input_buffer_);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = saved_errno;
    LOG(ERROR) << "TCPConnection::handleRead errno:" << errno;
    handleError();
  }
}

void TCPConnection::handleWrite() {
  loop_->AssertInLoopThread();
  if (loop_->epoll_server()->HasRegisterWrite(socket_->fd())) {
    ssize_t n = socket_->Write(output_buffer_.BeginRead(),
                               output_buffer_.ReadableBytes());
    if (n > 0) {
      output_buffer_.SkipReadBytes(n);
      if (output_buffer_.ReadableBytes() == 0) {
        loop_->epoll_server()->StopWrite(socket_->fd());
        if (write_complete_callback_) {
          loop_->QueueInLoop(
              std::bind(write_complete_callback_, shared_from_this()));
        }
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else {
      LOG(ERROR) << "TCPConnection::handleWrite";
      // if (state_ == kDisconnecting)
      // {
      //   shutdownInLoop();
      // }
    }
  } else {
    LOG(INFO) << "Connection fd = " << socket_->fd()
              << " is down, no more writing";
  }
}

void TCPConnection::handleClose() {
  loop_->AssertInLoopThread();
  LOG(INFO) << "fd = " << socket_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);
  loop_->epoll_server()->UnregisterFD(socket_->fd());

  TCPConnectionPtr guard_this(shared_from_this());
  connection_callback_(guard_this);
  // must be the last line
  close_callback_(guard_this);

  LOG(INFO) << "Connection handleClose " << socket_->fd();
}

void TCPConnection::handleError() {
  int err = socket_->GetSocketError();
  LOG(ERROR) << "TCPConnection::handleError [" << name_
             << "] - SO_ERROR = " << err << " " << safe_strerror(err);
}

}  // namespace raner
