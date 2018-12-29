// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raner/byte_buffer.h"

#include <errno.h>
#include <sys/uio.h>

namespace raner {
ssize_t ByteBuffer::ReadFD(int fd, int* save_errno) {
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536];
  struct iovec vec[2];
  const size_t writable = WritableBytes();
  vec[0].iov_base = begin() + writer_index_;
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof(extrabuf);
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
  const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
  const ssize_t n = ::readv(fd, vec, iovcnt);
  if (n < 0) {
    *save_errno = errno;
  } else if (static_cast<size_t>(n) <= writable) {
    writer_index_ += n;
  } else {
    writer_index_ = buffer_.size();
    Write(extrabuf, n - writable);
  }
  return n;
}
}  // namespace raner
