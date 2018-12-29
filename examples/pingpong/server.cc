#include "raner/tcp_server.h"

#include "raner/event_loop.h"
#include "raner/socket.h"

#include <glog/logging.h>
#include <stdio.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <utility>

using namespace raner;

void onConnection(const TCPConnectionPtr& conn) {
  if (conn->Connected()) {
    conn->SetTCPNoDelay();
  }
}

void onMessage(const TCPConnectionPtr& conn, ByteBuffer* buf) {
  conn->Send(buf);
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  if (argc < 4) {
    fprintf(stderr, "Usage: server <address> <port> <threads>\n");
  } else {
    LOG(INFO) << "pid = " << getpid()
              << ", tid = " << std::this_thread::get_id();

    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    int thread_count = atoi(argv[3]);

    EventLoop loop;
    TCPServer server(&loop, ip, port, "pingpong-server");

    server.SetConnectionCallback(onConnection);
    server.SetMessageCallback(onMessage);

    if (thread_count > 1) {
      server.SetThreadNum(thread_count);
    }

    server.Start();

    loop.Loop();
  }
}
