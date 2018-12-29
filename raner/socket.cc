// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/socket.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "raner/macros.h"
#include "raner/safe_strerror.h"

namespace {
bool FamilyIsTCP(int family) { return family == AF_INET || family == AF_INET6; }
}  // namespace

namespace raner {

// static
void Socket::CloseFD(int fd) {
  const int errno_copy = errno;
  if (IGNORE_EINTR(close(fd)) < 0) {
    LOG(ERROR) << "close: " << safe_strerror(errno);
    errno = errno_copy;
  }
}

// static
int Socket::DisableNagle(int fd) {
  int on = 1;
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

Socket::Socket()
    : fd_(-1),
      port_(0),
      socket_error_(false),
      family_(AF_INET),
      addr_ptr_(reinterpret_cast<sockaddr *>(&addr_.addr4)),
      addr_len_(sizeof(sockaddr)) {
  memset(&addr_, 0, sizeof(addr_));
}

Socket::~Socket() { Close(); }

bool Socket::BindAndListen(const std::string &host, int port) {
  errno = 0;
  if (!init(host, port) || !bindAndListen()) {
    Close();
    return false;
  }
  return true;
}

bool Socket::Connect(const std::string &host, int port) {
  errno = 0;
  if (!init(host, port) || !connect()) {
    Close();
    return false;
  }
  return true;
}

void Socket::Shutdown() {
  if (!IsClosed()) {
    PRESERVE_ERRNO_HANDLE_EINTR(shutdown(fd_, SHUT_RDWR));
  }
}

void Socket::ShutdownWrite() {
  if (!IsClosed()) {
    PRESERVE_ERRNO_HANDLE_EINTR(shutdown(fd_, SHUT_WR));
  }
}

void Socket::Close() {
  if (!IsClosed()) {
    CloseFD(fd_);
    fd_ = -1;
  }
}

bool Socket::initInternal() {
  fd_ = socket(family_, SOCK_STREAM, 0);
  if (fd_ < 0) {
    LOG(ERROR) << "initInternal " << safe_strerror(errno);
    return false;
  }
  DisableNagle(fd_);
  SetReuseAddr(true);
  if (!SetNonBlocking()) return false;
  return true;
}

bool Socket::SetNonBlocking() {
  const int flags = fcntl(fd_, F_GETFL);
  if (flags < 0) {
    LOG(ERROR) << "fcntl " << safe_strerror(errno);
    return false;
  }
  if (flags & O_NONBLOCK) return true;
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
    LOG(ERROR) << "fcntl " << safe_strerror(errno);
    return false;
  }
  return true;
}

int Socket::SetTCPNoDelay() { return DisableNagle(fd_); }

int Socket::SetReuseAddr(bool reuse) {
  int reuse_addr = reuse ? 1 : 0;
  return setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                    sizeof(reuse_addr));
}

int Socket::SetKeepAlive(bool enable) {
  int on = enable ? 1 : 0;
  return setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
}

bool Socket::init(const std::string &host, int port) {
  port_ = port;
  if (host.empty()) {
    // Use localhost: INADDR_LOOPBACK
    family_ = AF_INET;
    addr_.addr4.sin_family = static_cast<sa_family_t>(family_);
    addr_.addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else if (!resolve(host)) {
    return false;
  }
  CHECK(FamilyIsTCP(family_)) << "Invalid socket family.";
  if (family_ == AF_INET) {
    addr_.addr4.sin_port = htons(static_cast<int16_t>(port_));
    addr_ptr_ = reinterpret_cast<sockaddr *>(&addr_.addr4);
    addr_len_ = sizeof(addr_.addr4);
  } else if (family_ == AF_INET6) {
    addr_.addr6.sin6_port = htons(static_cast<int16_t>(port_));
    addr_ptr_ = reinterpret_cast<sockaddr *>(&addr_.addr6);
    addr_len_ = sizeof(addr_.addr6);
  }
  return initInternal();
}

bool Socket::bindAndListen() {
  errno = 0;
  if (HANDLE_EINTR(bind(fd_, addr_ptr_, addr_len_)) < 0 ||
      HANDLE_EINTR(listen(fd_, SOMAXCONN)) < 0) {
    LOG(ERROR) << "bindAndListen " << safe_strerror(errno);
    setSocketError();
    return false;
  }
  if (port_ == 0 && FamilyIsTCP(family_)) {
    SockAddr addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = 0;
    sockaddr *addr_ptr = NULL;
    uint16_t *port_ptr = NULL;
    if (family_ == AF_INET) {
      addr_ptr = reinterpret_cast<sockaddr *>(&addr.addr4);
      port_ptr = &addr.addr4.sin_port;
      addr_len = sizeof(addr.addr4);
    } else if (family_ == AF_INET6) {
      addr_ptr = reinterpret_cast<sockaddr *>(&addr.addr6);
      port_ptr = &addr.addr6.sin6_port;
      addr_len = sizeof(addr.addr6);
    }
    errno = 0;
    if (getsockname(fd_, addr_ptr, &addr_len) != 0) {
      LOG(ERROR) << "getsockname " << safe_strerror(errno);
      setSocketError();
      return false;
    }
    port_ = ntohs(*port_ptr);
  }
  return true;
}

bool Socket::Accept(Socket *new_socket) {
  DCHECK(new_socket != NULL);
  errno = 0;
  SockAddr addr;
  struct sockaddr *addr_ptr = reinterpret_cast<struct sockaddr *>(&addr);
  socklen_t addr_len = static_cast<socklen_t>(sizeof(addr));
  int new_fd = HANDLE_EINTR(accept(fd_, addr_ptr, &addr_len));
  if (new_fd < 0) {
    setSocketError();
    return false;
  }
  // DisableNagle(new_fd);

  new_socket->fd_ = new_fd;
  new_socket->addr_ = addr;
  new_socket->addr_len_ = addr_len;
  if (addr_len == sizeof(addr.addr4)) {
    new_socket->addr_ptr_ = reinterpret_cast<sockaddr *>(&addr_.addr4);
    new_socket->port_ = ntohs(addr.addr4.sin_port);
    new_socket->family_ = AF_INET;
  } else if (addr_len == sizeof(addr.addr6)) {
    new_socket->addr_ptr_ = reinterpret_cast<sockaddr *>(&addr_.addr6);
    new_socket->port_ = ntohs(addr.addr6.sin6_port);
    new_socket->family_ = AF_INET6;
  }

  if (!new_socket->SetNonBlocking()) return false;
  return true;
}

bool Socket::connect() {
  DCHECK(fcntl(fd_, F_GETFL) & O_NONBLOCK);
  errno = 0;
  if (HANDLE_EINTR(::connect(fd_, addr_ptr_, addr_len_)) < 0 &&
      errno != EINPROGRESS) {
    setSocketError();
    return false;
  }
  return true;
}

bool Socket::resolve(const std::string &host) {
  struct addrinfo hints;
  struct addrinfo *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;

  int errcode = getaddrinfo(host.c_str(), NULL, &hints, &res);  // FIXME block
  if (errcode != 0) {
    errno = 0;
    setSocketError();
    freeaddrinfo(res);
    return false;
  }
  family_ = res->ai_family;
  switch (res->ai_family) {
    case AF_INET:
      memcpy(&addr_.addr4, reinterpret_cast<sockaddr_in *>(res->ai_addr),
             sizeof(sockaddr_in));
      break;
    case AF_INET6:
      memcpy(&addr_.addr6, reinterpret_cast<sockaddr_in6 *>(res->ai_addr),
             sizeof(sockaddr_in6));
      break;
  }
  freeaddrinfo(res);
  return true;
}

int Socket::GetPort() {
  if (!FamilyIsTCP(family_)) {
    LOG(ERROR) << "Can't call GetPort() on an unix domain socket.";
    return 0;
  }
  return port_;
}

int Socket::GetSocketError() {
  int socket_errno;
  socklen_t opt_len = sizeof(socket_errno);
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &socket_errno, &opt_len) < 0) {
    return errno;
  } else {
    return socket_errno;
  }
}

void Socket::setSocketError() {
  socket_error_ = true;
  DCHECK_NE(EAGAIN, errno);
  DCHECK_NE(EWOULDBLOCK, errno);
  Close();
}

int Socket::Read(void *buffer, size_t buffer_size) {
  int ret = HANDLE_EINTR(static_cast<int>(read(fd_, buffer, buffer_size)));
  if (ret < 0) {
    LOG(ERROR) << "Read " << safe_strerror(errno);
    setSocketError();
  }
  return ret;
}

int Socket::NonBlockingRead(void *buffer, size_t buffer_size) {
  DCHECK(fcntl(fd_, F_GETFL) & O_NONBLOCK);
  int ret = HANDLE_EINTR(static_cast<int>(read(fd_, buffer, buffer_size)));
  if (ret < 0) {
    LOG(ERROR) << "NonBlockingRead " << safe_strerror(errno);
    setSocketError();
  }
  return ret;
}

int Socket::ReadNumBytes(void *buffer, size_t num_bytes) {
  size_t bytes_read = 0;
  int ret = 1;
  while (bytes_read < num_bytes && ret > 0) {
    ret =
        Read(static_cast<char *>(buffer) + bytes_read, num_bytes - bytes_read);
    if (ret >= 0) bytes_read += ret;
  }
  return static_cast<int>(bytes_read);
}

int Socket::Write(const void *buffer, size_t count) {
  int ret =
      HANDLE_EINTR(static_cast<int>(send(fd_, buffer, count, MSG_NOSIGNAL)));
  if (ret < 0) {
    LOG(ERROR) << "Write " << safe_strerror(errno);
    setSocketError();
  }
  return ret;
}

int Socket::NonBlockingWrite(const void *buffer, size_t count) {
  DCHECK(fcntl(fd_, F_GETFL) & O_NONBLOCK);
  int ret =
      HANDLE_EINTR(static_cast<int>(send(fd_, buffer, count, MSG_NOSIGNAL)));
  if (ret < 0) {
    LOG(ERROR) << "NonBlockingWrite " << safe_strerror(errno);
    setSocketError();
  }
  return ret;
}

int Socket::WriteString(const std::string &buffer) {
  return WriteNumBytes(buffer.c_str(), buffer.size());
}

int Socket::WriteNumBytes(const void *buffer, size_t num_bytes) {
  size_t bytes_written = 0;
  int ret = 1;
  while (bytes_written < num_bytes && ret > 0) {
    ret = Write(static_cast<const char *>(buffer) + bytes_written,
                num_bytes - bytes_written);
    if (ret >= 0) bytes_written += ret;
  }
  return static_cast<int>(bytes_written);
}

std::string Socket::GetLocalAddr() {
  char buf[64] = "";
  socklen_t size = 64;

  uint16_t port = 0;
  if (addr_ptr_->sa_family == AF_INET) {
    struct sockaddr_in *addr4 =
        reinterpret_cast<struct sockaddr_in *>(addr_ptr_);
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, size);
    port = ntohs(addr4->sin_port);
  } else if (addr_ptr_->sa_family == AF_INET6) {
    struct sockaddr_in6 *addr6 =
        reinterpret_cast<struct sockaddr_in6 *>(addr_ptr_);
    ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, size);
    port = ntohs(addr6->sin6_port);
  }

  size_t end = strlen(buf);
  assert(size > end);
  snprintf(buf + end, size - end, ":%u", port);
  return buf;
}

std::string Socket::GetPeerAddr() {
  char buf[64] = "";
  socklen_t size = 64;

  SockAddr addr;
  memset(&addr, 0, sizeof(addr));
  socklen_t addr_len = 0;
  sockaddr *addr_ptr = nullptr;
  if (family_ == AF_INET) {
    addr_ptr = reinterpret_cast<sockaddr *>(&addr.addr4);
    addr_len = sizeof(sockaddr_in);
  } else if (family_ == AF_INET6) {
    addr_ptr = reinterpret_cast<sockaddr *>(&addr.addr6);
    addr_len = sizeof(sockaddr_in6);
  }

  if (getpeername(fd_, addr_ptr, &addr_len) != 0) {
    LOG(ERROR) << "getpeername " << safe_strerror(errno);
    setSocketError();
    return buf;
  }

  uint16_t port = 0;
  if (family_ == AF_INET) {
    struct sockaddr_in *addr4 =
        reinterpret_cast<struct sockaddr_in *>(addr_ptr);
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, size);
    port = ntohs(addr4->sin_port);
  } else if (family_ == AF_INET6) {
    struct sockaddr_in6 *addr6 =
        reinterpret_cast<struct sockaddr_in6 *>(addr_ptr);
    ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, size);
    port = ntohs(addr6->sin6_port);
  }

  size_t end = strlen(buf);
  assert(size > end);
  snprintf(buf + end, size - end, ":%u", port);
  return buf;
}

bool Socket::GetTCPInfo(struct tcp_info *tcpi) const {
  socklen_t len = sizeof(*tcpi);
  bzero(tcpi, len);
  return ::getsockopt(fd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::GetTCPInfoString(char *buf, int len) const {
  struct tcp_info tcpi;
  bool ok = GetTCPInfo(&tcpi);
  if (ok) {
    snprintf(
        buf, len,
        "unrecovered=%u "
        "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
        "lost=%u retrans=%u rtt=%u rttvar=%u "
        "sshthresh=%u cwnd=%u total_retrans=%u",
        tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
        tcpi.tcpi_rto,          // Retransmit timeout in usec
        tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
        tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss,
        tcpi.tcpi_lost,     // Lost packets
        tcpi.tcpi_retrans,  // Retransmitted packets out
        tcpi.tcpi_rtt,      // Smoothed round trip time in usec
        tcpi.tcpi_rttvar,   // Medium deviation
        tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
        tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
  }
  return ok;
}

}  // namespace raner
