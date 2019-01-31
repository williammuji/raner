#include "examples/ttcp/common.h"

#include <glog/logging.h>

#include "raner/event_loop.h"
#include "raner/tcp_client.h"
#include "raner/tcp_server.h"

#include <stdio.h>

using namespace raner;

EventLoop* g_loop;

struct Context {
  int count;
  int64_t bytes;
  SessionMessage session;
  ByteBuffer output;

  Context() : count(0), bytes(0) {
    session.number = 0;
    session.length = 0;
  }
};

/////////////////////////////////////////////////////////////////////
// T R A N S M I T
/////////////////////////////////////////////////////////////////////

namespace trans {

void onConnection(const TCPConnectionPtr& conn) {
  if (conn->Connected()) {
    printf("connected\n");
    Context context;
    context.count = 1;
    context.bytes = FLAGS_length;
    context.session.number = FLAGS_number;
    context.session.length = FLAGS_length;
    context.output.WriteInt32(FLAGS_length);
    context.output.EnsureWritableBytes(FLAGS_length);
    for (int i = 0; i < FLAGS_length; ++i) {
      context.output.BeginWrite()[i] = "0123456789ABCDEF"[i % 16];
    }
    context.output.SkipWriteBytes(FLAGS_length);
    conn->SetContext(context);

    SessionMessage sessionMessage = {0, 0};
    sessionMessage.number = htonl(FLAGS_number);
    sessionMessage.length = htonl(FLAGS_length);
    conn->Send(&sessionMessage, sizeof(sessionMessage));

    conn->Send(context.output.ToStringView());
  } else {
    const Context& context = std::any_cast<Context>(conn->GetContext());
    LOG(INFO) << "payload bytes " << context.bytes;
    conn->GetLoop()->Quit();
  }
}

void onMessage(const TCPConnectionPtr& conn, ByteBuffer* buf) {
  Context* context = std::any_cast<Context>(conn->GetMutableContext());
  while (buf->ReadableBytes() >= sizeof(int32_t)) {
    int32_t length = buf->ReadInt32();
    if (length == context->session.length) {
      if (context->count < context->session.number) {
        conn->Send(context->output.ToStringView());
        ++context->count;
        context->bytes += length;
      } else {
        conn->Shutdown();
        break;
      }
    } else {
      conn->Shutdown();
      break;
    }
  }
}

}  // namespace trans

void transmit() {
  raner::Time start(raner::Time::Now());
  EventLoop loop;
  g_loop = &loop;
  TCPClient client(&loop, FLAGS_host, FLAGS_port, "TCPTransmit");
  client.SetConnectionCallback(std::bind(&trans::onConnection, _1));
  client.SetMessageCallback(std::bind(&trans::onMessage, _1, _2));
  client.Connect();
  loop.Loop();
  int64_t elapsed = durationSeconds(Time::Now() - start);
  double total_mb = 1.0 * FLAGS_length * FLAGS_number / 1024 / 1024;
  printf("%.3f MiB transferred\n%.3f MiB/s\n", total_mb,
         total_mb / static_cast<double>(elapsed));
}

/////////////////////////////////////////////////////////////////////
// R E C E I V E
/////////////////////////////////////////////////////////////////////

namespace receiving {

void onConnection(const TCPConnectionPtr& conn) {
  if (conn->Connected()) {
    Context context;
    conn->SetContext(context);
  } else {
    const Context& context = std::any_cast<Context>(conn->GetContext());
    LOG(INFO) << "payload bytes " << context.bytes;
    conn->GetLoop()->Quit();
  }
}

void onMessage(const TCPConnectionPtr& conn, ByteBuffer* buf) {
  while (buf->ReadableBytes() >= sizeof(int32_t)) {
    Context* context = std::any_cast<Context>(conn->GetMutableContext());
    SessionMessage& session = context->session;
    if (session.number == 0 && session.length == 0) {
      if (buf->ReadableBytes() >= sizeof(SessionMessage)) {
        session.number = buf->ReadInt32();
        session.length = buf->ReadInt32();
        context->output.WriteInt32(session.length);
        printf("receive number = %d\nreceive length = %d\n", session.number,
               session.length);
      } else {
        break;
      }
    } else {
      const unsigned total_len =
          session.length + static_cast<int>(sizeof(int32_t));
      const int32_t length = buf->PeekInt32();
      if (length == session.length) {
        if (buf->ReadableBytes() >= total_len) {
          buf->SkipReadBytes(total_len);
          conn->Send(context->output.ToStringView());
          ++context->count;
          context->bytes += length;
          if (context->count >= session.number) {
            conn->Shutdown();
            break;
          }
        } else {
          break;
        }
      } else {
        printf("wrong length %d\n", length);
        conn->Shutdown();
        break;
      }
    }
  }
}

}  // namespace receiving

void receive() {
  EventLoop loop;
  g_loop = &loop;
  TCPServer server(&loop, FLAGS_host, FLAGS_port, "TCPReceive");
  server.SetConnectionCallback(std::bind(&receiving::onConnection, _1));
  server.SetMessageCallback(std::bind(&receiving::onMessage, _1, _2));
  server.Start();
  loop.Loop();
}
