#include "codec.h"

#include <glog/logging.h>

#include "raner/event_loop.h"
#include "raner/tcp_server.h"

#include <stdio.h>
#include <unistd.h>
#include <set>
#include <string>

using namespace raner;

class ChatServer {
 public:
  ChatServer(EventLoop* loop, std::string_view ip, uint16_t port)
      : server_(loop, ip, port, "ChatServer"),
        codec_(std::bind(&ChatServer::onStringMessage, this, _1, _2)) {
    server_.SetConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));
    server_.SetMessageCallback(
        std::bind(&LengthHeaderCodec::OnMessage, &codec_, _1, _2));
  }

  void SetThreadNum(int num_threads) { server_.SetThreadNum(num_threads); }

  void Start() { server_.Start(); }

 private:
  void onConnection(const TCPConnectionPtr& conn) {
    LOG(INFO) << conn->GetLocalAddr() << " -> " << conn->GetPeerAddr() << " is "
              << (conn->Connected() ? "UP" : "DOWN");

    std::lock_guard<std::mutex> lock(mutex_);
    if (conn->Connected()) {
      connections_.insert(conn);
    } else {
      connections_.erase(conn);
    }
  }

  void onStringMessage(const TCPConnectionPtr&, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (ConnectionList::iterator it = connections_.begin();
         it != connections_.end(); ++it) {
      codec_.Send((*it).get(), message);
    }
  }

  typedef std::set<TCPConnectionPtr> ConnectionList;
  TCPServer server_;
  LengthHeaderCodec codec_;
  std::mutex mutex_;
  ConnectionList connections_;

  DISALLOW_COPY_AND_ASSIGN(ChatServer);
};

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "pid = " << getpid();
  if (argc > 1) {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    ChatServer server(&loop, "", port);
    if (argc > 2) {
      server.SetThreadNum(atoi(argv[2]));
    }
    server.Start();
    loop.Loop();
  } else {
    printf("Usage: %s port [thread_num]\n", argv[0]);
  }
}
