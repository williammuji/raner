#include "codec.h"

#include <glog/logging.h>

#include "raner/event_loop.h"
#include "raner/tcp_server.h"

#include <stdio.h>
#include <unistd.h>
#include <set>
#include <string_view>

using namespace raner;

typedef std::set<TCPConnectionPtr> ConnectionList;
typedef std::unique_ptr<ConnectionList> ConnectionListPtr;
thread_local ConnectionListPtr localConnections;

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

  void Start() {
    server_.SetThreadInitCallback(std::bind(&ChatServer::threadInit, this, _1));
    server_.Start();
  }

 private:
  void onConnection(const TCPConnectionPtr& conn) {
    LOG(INFO) << conn->GetLocalAddr() << " -> " << conn->GetPeerAddr() << " is "
              << (conn->Connected() ? "UP" : "DOWN");

    if (conn->Connected()) {
      localConnections->insert(conn);
    } else {
      localConnections->erase(conn);
    }
  }

  void onStringMessage(const TCPConnectionPtr&, const std::string& message) {
    EventLoop::Functor f =
        std::bind(&ChatServer::distributeMessage, this, message);
    LOG(INFO) << "onStringMessage enter";

    std::lock_guard<std::mutex> lock(mutex_);
    for (std::set<EventLoop*>::iterator it = loops_.begin(); it != loops_.end();
         ++it) {
      (*it)->QueueInLoop(f);
    }
    LOG(INFO) << "onStringMessage exit";
  }

  void distributeMessage(const std::string& message) {
    LOG(INFO) << "begin";
    for (ConnectionList::iterator it = localConnections->begin();
         it != localConnections->end(); ++it) {
      codec_.Send((*it).get(), message);
    }
    LOG(INFO) << "end";
  }

  void threadInit(EventLoop* loop) {
    assert(localConnections == nullptr);
    localConnections = std::make_unique<ConnectionList>();
    assert(localConnections != nullptr);
    std::lock_guard<std::mutex> lock(mutex_);
    loops_.insert(loop);
  }

  TCPServer server_;
  LengthHeaderCodec codec_;

  std::mutex mutex_;
  std::set<EventLoop*> loops_;
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
