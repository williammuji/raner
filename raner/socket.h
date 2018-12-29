// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_SOCKET_H_
#define RANER_NET_SOCKET_H_

#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string>
#include <vector>

#include "raner/macros.h"

struct tcp_info;

namespace raner {

class Socket {
 public:
  Socket();
  ~Socket();

  bool BindAndListen(const std::string &host, int port);
  bool Connect(const std::string &host, int port);

  void Shutdown();
  void ShutdownWrite();

  void Close();
  bool IsClosed() const { return fd_ < 0; }

  int fd() const { return fd_; }

  bool Accept(Socket *new_socket);

  // Returns the port allocated to this socket or zero on error.
  int GetPort();

  int GetSocketError();

  // Just a wrapper around unix read() function.
  // Reads up to buffer_size, but may read less than buffer_size.
  // Returns the number of bytes read.
  int Read(void *buffer, size_t buffer_size);
  int NonBlockingRead(void *buffer, size_t buffer_size);

  // Calls Read() multiple times until num_bytes is written to the provided
  // buffer. No bounds checking is performed.
  // Returns number of bytes read, which can be different from num_bytes in case
  // of errror.
  int ReadNumBytes(void *buffer, size_t num_bytes);

  // Wrapper around send().
  int Write(const void *buffer, size_t count);
  int NonBlockingWrite(const void *buffer, size_t count);

  // Calls Write() multiple times until num_bytes is written. No bounds checking
  // is performed. Returns number of bytes written, which can be different from
  // num_bytes in case of errror.
  int WriteNumBytes(const void *buffer, size_t num_bytes);

  // Calls WriteNumBytes for the given std::string. Note that the null
  // terminator is not written to the socket.
  int WriteString(const std::string &buffer);

  bool has_error() const { return socket_error_; }

  bool SetNonBlocking();
  int SetTCPNoDelay();
  int SetReuseAddr(bool reuse);
  int SetKeepAlive(bool enable);

  std::string GetLocalAddr();
  std::string GetPeerAddr();

  bool GetTCPInfo(struct tcp_info *) const;
  bool GetTCPInfoString(char *buf, int len) const;

  static void CloseFD(int fd);
  static int DisableNagle(int socket);

 private:
  union SockAddr {
    // IPv4 sockaddr
    sockaddr_in addr4;
    // IPv6 sockaddr
    sockaddr_in6 addr6;
  };

  // If |host| is empty, use localhost.
  bool init(const std::string &host, int port);
  bool bindAndListen();
  bool connect();

  bool resolve(const std::string &host);
  bool initInternal();
  void setSocketError();

  int fd_;
  // Listen port
  // Port to connect
  // Client socket port
  int port_;
  bool socket_error_;

  // Family of the socket (AF_INET, AF_INET6).
  int family_;

  SockAddr addr_;

  // Points to one of the members of the above union depending on the family.
  sockaddr *addr_ptr_;
  // Length of one of the members of the above union depending on the family.
  socklen_t addr_len_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

}  // namespace raner

#endif  // RANER_NET_SOCKET_H_
