// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/tcp_server.h"

#include <glog/logging.h>
#include "raner/event_loop.h"
#include "raner/event_loop_thread_pool.h"
#include "raner/socket.h"

#include <stdio.h>  // snprintf

namespace {
const int kEpollFlags = EPOLLIN;
}  // namespace

namespace raner {

TCPServer::TCPServer(EventLoop *loop, std::string_view host, int port,
                     std::string_view name)
    : loop_(CHECK_NOTNULL(loop)),
      host_(host),
      port_(port),
      name_(name),
      listenning_(false),
      idle_fd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)),
      thread_pool_(new EventLoopThreadPool(loop, name_)),
      connection_callback_(defaultConnectionCallback),
      message_callback_(defaultMessageCallback),
      started_(0),
      next_conn_id_(1) {
  assert(idle_fd_ >= 0);
}

TCPServer::~TCPServer() {
  loop_->AssertInLoopThread();
  LOG(INFO) << "TCPServer::~TCPServer [" << name_ << "] destructing";

  for (auto &item : connections_) {
    TCPConnectionPtr conn(item.second);
    item.second.reset();
    conn->GetLoop()->RunInLoop(
        std::bind(&TCPConnection::ConnectDestroyed, conn));
  }

  loop_->epoll_server()->UnregisterFD(socket_->fd());
  ::close(idle_fd_);
}

void TCPServer::createSocketAndListen() {
  loop_->AssertInLoopThread();

  socket_.reset(new Socket());  // FIXME move to constructor
  if (!socket_->BindAndListen(host_, port_)) {
    LOG(FATAL) << "TCPServer could not bind and listen to " << host_ << ":"
               << port_;
    return;
  }
  listenning_ = true;
  loop_->epoll_server()->RegisterFD(socket_->fd(), this, kEpollFlags);
}

void TCPServer::handleRead() {
  loop_->AssertInLoopThread();

  std::unique_ptr<Socket> client_socket(new Socket());
  bool res = socket_->Accept(client_socket.get());
  if (res) {
    newConnection(std::move(client_socket));
  } else {  // if (res)
    LOG(ERROR) << "in TCPServer::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE) {
      ::close(idle_fd_);
      idle_fd_ = ::accept(socket_->fd(), NULL, NULL);
      ::close(idle_fd_);
      idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

void TCPServer::OnEvent(int fd, EpollEvent *event) {
  loop_->AssertInLoopThread();

  if (event->in_events & EPOLLIN) {
    LOG(INFO) << "OnEvent EPOLLIN fd:" << fd;
    handleRead();
  }

  if (event->in_events & EPOLLERR) {
    LOG(INFO) << "OnEvent EPOLLERR fd:" << fd;
  }
}

void TCPServer::SetThreadNum(int num_threads) {
  assert(0 <= num_threads);
  thread_pool_->SetThreadNum(num_threads);
}

void TCPServer::Start() {
  if (started_.fetch_add(1) == 0) {
    thread_pool_->Start(thread_init_callback_);

    assert(!listenning_);
    loop_->RunInLoop(std::bind(&TCPServer::createSocketAndListen, this));
  }
}

void TCPServer::newConnection(std::unique_ptr<Socket> client_socket) {
  loop_->AssertInLoopThread();
  EventLoop *io_loop = thread_pool_->GetNextLoop();
  char buf[64];
  snprintf(buf, sizeof(buf), "-%s:%d#%d", host_.c_str(), port_, next_conn_id_);
  ++next_conn_id_;
  std::string conn_name = name_ + buf;

  LOG(INFO) << "TCPServer::newConnection [" << name_ << "] - new connection ["
            << conn_name << "] from " << client_socket->GetPeerAddr();

  TCPConnectionPtr conn(
      new TCPConnection(io_loop, conn_name, std::move(client_socket)));
  connections_[conn_name] = conn;
  conn->SetConnectionCallback(connection_callback_);
  conn->SetMessageCallback(message_callback_);
  conn->SetWriteCompleteCallback(write_complete_callback_);
  conn->SetCloseCallback(
      std::bind(&TCPServer::removeConnection, this, _1));  // FIXME: unsafe
  io_loop->RunInLoop(std::bind(&TCPConnection::ConnectEstablished, conn));
}

void TCPServer::removeConnection(const TCPConnectionPtr &conn) {
  // FIXME: unsafe
  loop_->RunInLoop(std::bind(&TCPServer::removeConnectionInLoop, this, conn));
}

void TCPServer::removeConnectionInLoop(const TCPConnectionPtr &conn) {
  loop_->AssertInLoopThread();
  LOG(INFO) << "TCPServer::removeConnectionInLoop [" << name_
            << "] - connection " << conn->Name();
  size_t n = connections_.erase(conn->Name());
  (void)n;
  assert(n == 1);
  EventLoop *io_loop = conn->GetLoop();
  io_loop->QueueInLoop(std::bind(&TCPConnection::ConnectDestroyed, conn));
}

}  // namespace raner
