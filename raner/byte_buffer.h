// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_BYTE_BUFFER_H_
#define RANER_NET_BYTE_BUFFER_H_

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "raner/endian.h"

namespace raner {

// https://docs.jboss.org/netty/3.2/api/org/jboss/netty/buffer/ChannelBuffer.html
// +-------------------+------------------+------------------+
// | discardable bytes |  readable bytes  |  writable bytes  |
// |                   |     (CONTENT)    |                  |
// +-------------------+------------------+------------------+
// |                   |                  |                  |
// 0      <=      readerIndex   <=   writerIndex    <=    capacity

class ByteBuffer {
 public:
  static constexpr size_t kInitialSize = 1024;

  explicit ByteBuffer(size_t initial_size = kInitialSize)
      : buffer_(initial_size), reader_index_(0), writer_index_(0) {
    assert(DiscardableBytes() == 0);
    assert(ReadableBytes() == 0);
    assert(WritableBytes() == initial_size);
  }

  void Swap(ByteBuffer &rhs) {
    buffer_.swap(rhs.buffer_);
    std::swap(reader_index_, rhs.reader_index_);
    std::swap(writer_index_, rhs.writer_index_);
  }

  size_t DiscardableBytes() const { return reader_index_; }
  size_t ReadableBytes() const { return writer_index_ - reader_index_; }
  size_t WritableBytes() const { return buffer_.size() - writer_index_; }

  const char *BeginRead() const { return begin() + reader_index_; }
  const char *BeginWrite() const { return begin() + writer_index_; }
  char *BeginWrite() { return begin() + writer_index_; }

  const char *FindCRLF() const {
    const char *crlf = std::search(BeginRead(), BeginWrite(), kCRLF, kCRLF + 2);
    return crlf == BeginWrite() ? nullptr : crlf;
  }
  const char *FindCRLF(const char *start) const {
    assert(BeginRead() <= start);
    assert(start <= BeginWrite());
    const char *crlf = std::search(start, BeginWrite(), kCRLF, kCRLF + 2);
    return crlf == BeginWrite() ? nullptr : crlf;
  }

  const char *FindEOL() const {
    const void *eol = memchr(BeginRead(), '\n', ReadableBytes());
    return static_cast<const char *>(eol);
  }
  const char *FindEOL(const char *start) const {
    assert(BeginRead() <= start);
    assert(start <= BeginWrite());
    const void *eol = memchr(start, '\n', BeginWrite() - start);
    return static_cast<const char *>(eol);
  }

  void SkipReadBytes(size_t len) {
    assert(len <= ReadableBytes());
    if (len < ReadableBytes())
      reader_index_ += len;
    else
      SkipAll();
  }

  std::string SkipAllAsString() { return SkipAsString(ReadableBytes()); }

  std::string SkipAsString(size_t len) {
    assert(len <= ReadableBytes());
    std::string res(BeginRead(), len);
    SkipReadBytes(len);
    return res;
  }

  void SkipWriteBytes(size_t len) {
    assert(len <= WritableBytes());
    writer_index_ += len;
  }

  void Write(const char *data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, BeginWrite());
    SkipWriteBytes(len);
  }
  void Write(const void *data, size_t len) {
    Write(static_cast<const char *>(data), len);
  }
  void Write(std::string_view str) { Write(str.data(), str.size()); }

  void WriteInt64(int64_t x) {
    uint64_t n = ghtonll(static_cast<uint64_t>(x));
    Write(&n, sizeof(n));
  }

  void WriteInt32(int32_t x) {
    uint32_t n = ghtonl(static_cast<uint32_t>(x));
    Write(&n, sizeof(n));
  }

  void WriteInt16(int32_t x) {
    uint16_t n = ghtons(static_cast<uint16_t>(x));
    Write(&n, sizeof(n));
  }

  void WriteInt8(int8_t x) { Write(&x, sizeof(x)); }

  int8_t ReadInt8() {
    int8_t x = PeekInt8();
    SkipReadBytes(sizeof(int8_t));
    return x;
  }

  int16_t ReadInt16() {
    int16_t x = PeekInt16();
    SkipReadBytes(sizeof(int16_t));
    return x;
  }

  int32_t ReadInt32() {
    int32_t x = PeekInt32();
    SkipReadBytes(sizeof(int32_t));
    return x;
  }

  int64_t ReadInt64() {
    int64_t x = PeekInt64();
    SkipReadBytes(sizeof(int64_t));
    return x;
  }

  int8_t PeekInt8() const {
    assert(ReadableBytes() >= sizeof(int8_t));
    int8_t x = *BeginRead();
    return x;
  }

  int16_t PeekInt16() const {
    assert(ReadableBytes() >= sizeof(int16_t));
    int16_t x = 0;
    ::memcpy(&x, BeginRead(), sizeof(x));
    return static_cast<int16_t>(gntohs(static_cast<uint16_t>(x)));
  }

  int32_t PeekInt32() const {
    assert(ReadableBytes() >= sizeof(int32_t));
    int32_t x = 0;
    ::memcpy(&x, BeginRead(), sizeof(x));
    return static_cast<int32_t>(gntohl(static_cast<uint32_t>(x)));
  }

  int64_t PeekInt64() const {
    assert(ReadableBytes() >= sizeof(int64_t));
    int64_t x = 0;
    ::memcpy(&x, BeginRead(), sizeof(x));
    return static_cast<int64_t>(gntohll(static_cast<uint64_t>(x)));
  }

  std::string_view ToStringView() const {
    return std::string_view(BeginRead(), static_cast<int>(ReadableBytes()));
  }

  std::string ToString(size_t len) {
    assert(len <= ReadableBytes());
    std::string res(BeginRead(), len);
    SkipReadBytes(len);
    return res;
  }

  std::string ToString() { return ToString(ReadableBytes()); }

  void SkipAll() { reader_index_ = writer_index_ = 0; }

  void Shrink() {
    ByteBuffer other;
    other.ensureWritableBytes(ReadableBytes());
    other.Write(ToStringView());
    Swap(other);
  }

  ssize_t ReadFD(int fd, int *save_errno);

 private:
  char *begin() { return &*buffer_.begin(); }
  const char *begin() const { return &*buffer_.begin(); }

  void ensureWritableBytes(size_t len) {
    if (WritableBytes() < len) expandCapacity(len);
    assert(WritableBytes() >= len);
  }

  void expandCapacity(size_t len) {
    if (WritableBytes() + DiscardableBytes() < len) {
      buffer_.resize(writer_index_ + len);
    } else {
      assert(0 < reader_index_);
      size_t readable = ReadableBytes();
      std::copy(begin() + reader_index_, begin() + writer_index_, begin());
      reader_index_ = 0;
      writer_index_ = reader_index_ + readable;
      assert(readable == ReadableBytes());
    }
  }

 private:
  std::vector<char> buffer_;
  size_t reader_index_;
  size_t writer_index_;

  static constexpr char kCRLF[] = "\r\n";
};

}  // namespace raner

#endif  // RANER_NET_BYTE_BUFFER_H_
