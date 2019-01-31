#ifndef RANER_EXAMPLES_CHAT_CODEC_H
#define RANER_EXAMPLES_CHAT_CODEC_H

#include <glog/logging.h>
#include <string>

#include "raner/byte_buffer.h"
#include "raner/endian.h"
#include "raner/tcp_connection.h"

class LengthHeaderCodec {
 public:
  typedef std::function<void(const raner::TCPConnectionPtr&,
                             const std::string& message)>
      StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)
      : messageCallback_(cb) {}

  void OnMessage(const raner::TCPConnectionPtr& conn, raner::ByteBuffer* buf) {
    while (buf->ReadableBytes() >= kHeaderLen)  // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->BeginRead();
      uint32_t len = raner::gntohl(*static_cast<const uint32_t*>(data));
      if (len > 65536) {
        LOG(ERROR) << "Invalid length " << len;
        conn->Shutdown();  // FIXME: disable reading
        break;
      } else if (buf->ReadableBytes() >= len + kHeaderLen) {
        buf->SkipReadBytes(kHeaderLen);
        std::string message(buf->BeginRead(), len);
        messageCallback_(conn, message);
        buf->SkipReadBytes(len);
      } else {
        break;
      }
    }
  }

  // FIXME: TCPConnectionPtr
  void Send(raner::TCPConnection* conn, std::string_view message) {
    raner::ByteBuffer buf;
    uint32_t len = raner::ghtonl(static_cast<uint32_t>(message.size()));
    buf.Write(&len, sizeof(len));
    buf.Write(message.data(), message.size());
    conn->Send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(uint32_t);

  DISALLOW_COPY_AND_ASSIGN(LengthHeaderCodec);
};

#endif  // RANER_EXAMPLES_CHAT_CODEC_H
