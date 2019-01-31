#include "codec.h"

#include <glog/logging.h>
#include <atomic>
#include <mutex>

#include "raner/event_loop.h"
#include "raner/event_loop_thread_pool.h"
#include "raner/tcp_client.h"

#include <stdio.h>
#include <unistd.h>

using namespace raner;

int g_connections = 0;
std::atomic<int32_t> g_aliveConnections;
std::atomic<int32_t> g_messagesReceived;
Time g_startTime;
std::vector<Time> g_receiveTime;
EventLoop* g_loop;
std::function<void()> g_statistic;

class ChatClient {
 public:
  ChatClient(EventLoop* loop, std::string_view ip, uint16_t port)
      : loop_(loop),
        client_(loop, ip, port, "LoadTestClient"),
        codec_(std::bind(&ChatClient::onStringMessage, this, _1, _2)) {
    client_.SetConnectionCallback(
        std::bind(&ChatClient::onConnection, this, _1));
    client_.SetMessageCallback(
        std::bind(&LengthHeaderCodec::OnMessage, &codec_, _1, _2));
    // client_.EnableRetry();
  }

  void Connect() { client_.Connect(); }

  void Disconnect() {
    // client_.Disconnect();
  }

  Time receive_time() const { return receive_time_; }

 private:
  void onConnection(const TCPConnectionPtr& conn) {
    LOG(INFO) << conn->GetLocalAddr() << " -> " << conn->GetPeerAddr() << " is "
              << (conn->Connected() ? "UP" : "DOWN");

    if (conn->Connected()) {
      connection_ = conn;
      ++g_aliveConnections;
      if (g_aliveConnections == g_connections) {
        LOG(INFO) << "all connected";

        send_timer_ = loop_->CreateTimer(std::bind(&ChatClient::send, this));
        send_timer_->Set(Time::Now() + Duration(10 * 1000 * 1000));
      }
    } else {
      connection_.reset();
    }
  }

  void onStringMessage(const TCPConnectionPtr&, const std::string& message) {
    // printf("<<< %s\n", message.c_str());
    receive_time_ = Time::Now();
    ++g_messagesReceived;
    int received = g_messagesReceived;
    if (received == g_connections) {
      Time end_time = Time::Now();
      LOG(INFO) << "all received " << g_connections << " in "
                << durationSeconds(end_time - g_startTime) << " seconds";
      g_loop->QueueInLoop(g_statistic);
    } else if (received % 1000 == 0) {
      LOG(INFO) << received;
    }
  }

  void send() {
    g_startTime = Time::Now();
    codec_.Send(connection_.get(), "hello");
    LOG(INFO) << "sent";
  }

  EventLoop* loop_;
  TCPClient client_;
  LengthHeaderCodec codec_;
  TCPConnectionPtr connection_;
  Time receive_time_;
  std::unique_ptr<EpollTimer> send_timer_;

  DISALLOW_COPY_AND_ASSIGN(ChatClient);
};

void statistic(const std::vector<std::unique_ptr<ChatClient>>& clients) {
  LOG(INFO) << "statistic " << clients.size();
  std::vector<double> seconds(clients.size());
  for (size_t i = 0; i < clients.size(); ++i) {
    auto secs = durationSeconds(clients[i]->receive_time() - g_startTime);
    seconds[i] = static_cast<double>(secs);
  }

  std::sort(seconds.begin(), seconds.end());
  for (size_t i = 0; i < clients.size();
       i += std::max(static_cast<size_t>(1), clients.size() / 20)) {
    printf("%6zd%% %.6f\n", i * 100 / clients.size(), seconds[i]);
  }
  if (clients.size() >= 100) {
    printf("%6d%% %.6f\n", 99, seconds[clients.size() - clients.size() / 100]);
  }
  printf("%6d%% %.6f\n", 100, seconds.back());
}

int main(int argc, char* argv[]) {
  LOG(INFO) << "pid = " << getpid();
  if (argc > 3) {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    g_connections = atoi(argv[3]);
    int threads = 0;
    if (argc > 4) {
      threads = atoi(argv[4]);
    }

    EventLoop loop;
    g_loop = &loop;
    EventLoopThreadPool loop_pool(&loop, "chat-loadtest");
    loop_pool.SetThreadNum(threads);
    loop_pool.Start();

    g_receiveTime.reserve(g_connections);
    std::vector<std::unique_ptr<ChatClient>> clients(g_connections);
    g_statistic = std::bind(statistic, std::ref(clients));

    for (int i = 0; i < g_connections; ++i) {
      clients[i].reset(new ChatClient(loop_pool.GetNextLoop(), argv[1], port));
      clients[i]->Connect();
      usleep(200);
    }

    loop.Loop();
    // client.disconnect();
  } else {
    printf("Usage: %s host_ip port connections [threads]\n", argv[0]);
  }
}
