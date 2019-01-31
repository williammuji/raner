#include "codec.h"

#include <glog/logging.h>

#include "raner/event_loop_thread.h"
#include "raner/tcp_client.h"
#include "raner/util.h"

#include <stdio.h>
#include <unistd.h>
#include <iostream>

using namespace raner;

class ChatClient {
 public:
  ChatClient(EventLoop* loop, std::string_view ip, uint16_t port)
      : client_(loop, ip, port, "ChatClient"),
        codec_(std::bind(&ChatClient::onStringMessage, this, _1, _2)) {
    client_.SetConnectionCallback(
        std::bind(&ChatClient::onConnection, this, _1));
    client_.SetMessageCallback(
        std::bind(&LengthHeaderCodec::OnMessage, &codec_, _1, _2));
    client_.EnableRetry();
  }

  void Connect() { client_.Connect(); }

  void Disconnect() { client_.Disconnect(); }

  void Write(std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
      codec_.Send(connection_.get(), message);
    }
  }

 private:
  void onConnection(const TCPConnectionPtr& conn) {
    LOG(INFO) << conn->GetLocalAddr() << " -> " << conn->GetPeerAddr() << " is "
              << (conn->Connected() ? "UP" : "DOWN");

    std::lock_guard<std::mutex> lock(mutex_);
    if (conn->Connected()) {
      connection_ = conn;
    } else {
      connection_.reset();
    }
  }

  void onStringMessage(const TCPConnectionPtr&, const std::string& message) {
    printf("<<< %s\n", message.c_str());
  }

  TCPClient client_;
  LengthHeaderCodec codec_;
  std::mutex mutex_;
  TCPConnectionPtr connection_;

  DISALLOW_COPY_AND_ASSIGN(ChatClient);
};

int main(int argc, char* argv[]) {
  LOG(INFO) << "pid = " << getpid();
  if (argc > 2) {
    EventLoopThread loop_thread;
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    ChatClient client(loop_thread.StartLoop(), argv[1], port);
    client.Connect();
    std::string line;
    while (std::getline(std::cin, line)) {
      client.Write(line);
    }
    client.Disconnect();
    SleepUsec(1000 * 1000);  // wait for disconnect, see ace/logging/client.cc
  } else {
    printf("Usage: %s host_ip port\n", argv[0]);
  }
}
