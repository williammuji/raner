#include "raner/tcp_client.h"

#include "raner/epoll_server.h"
#include "raner/event_loop.h"
#include "raner/event_loop_thread_pool.h"
#include <glog/logging.h>
#include <thread>

#include <atomic>
#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace raner;

class Client;

class Session {
public:
  Session(EventLoop *loop, std::string_view ip, int port, std::string_view name,
          Client *owner)
      : client_(loop, ip, port, name), owner_(owner), bytes_read_(0),
        bytes_written_(0), messages_read_(0) {
    client_.SetConnectionCallback(std::bind(&Session::onConnection, this, _1));
    client_.SetMessageCallback(std::bind(&Session::onMessage, this, _1, _2));
  }

  void Start() { client_.Start(); }

  void Stop() { client_.Disconnect(); }

  int64_t bytes_read() const { return bytes_read_; }

  int64_t messages_read() const { return messages_read_; }

private:
  void onConnection(const TCPConnectionPtr &conn);

  void onMessage(const TCPConnectionPtr &conn, ByteBuffer *buf) {
    ++messages_read_;
    bytes_read_ += buf->ReadableBytes();
    bytes_written_ += buf->ReadableBytes();
    conn->Send(buf);
  }

  TCPClient client_;
  Client *owner_;
  int64_t bytes_read_;
  int64_t bytes_written_;
  int64_t messages_read_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

class Client {
public:
  Client(EventLoop* loop, std::string_view ip, int port, int block_size,
         int session_count, int timeout, int thread_count)
      : loop_(loop),
        thread_pool_(loop, "pingpong-client"),
        session_count_(session_count),
        timeout_(timeout),
        num_connected_(0),
        timeout_timer_(loop->CreateTimer(std::bind(&Client::HandleTimeout, this))) {
    timeout_timer_->Update(Time::Now() + Duration(timeout_ * 1000 * 1000));
    if (thread_count > 1) {
    thread_pool_.SetThreadNum(thread_count);
    }
    thread_pool_.Start();

    for (int i = 0; i < block_size; ++i) {
    message_.push_back(static_cast<char>(i % 128));
    }

    for (int i = 0; i < session_count; ++i) {
    char buf[32];
    snprintf(buf, sizeof buf, "C%05d", i);
    Session *session =
        new Session(thread_pool_.GetNextLoop(), ip, port, buf, this);
    session->Start();
    sessions_.emplace_back(session);
    }
}

const std::string& message() const {
  return message_;
}

void OnConnect() {
  ++num_connected_;
  if (num_connected_ == session_count_) {
    LOG(WARNING) << "all connected";
  }
}

void OnDisconnect(const TCPConnectionPtr &conn) {
  --num_connected_;
  if (num_connected_ == 0) {
    LOG(WARNING) << "all disconnected";

    int64_t total_bytes_read = 0;
    int64_t total_message_read = 0;
    for (const auto &session : sessions_) {
      total_bytes_read += session->bytes_read();
      total_message_read += session->messages_read();
    }
    LOG(WARNING) << total_bytes_read << " total bytes read";
    LOG(WARNING) << total_message_read << " total messages read";
    LOG(WARNING) << static_cast<double>(total_bytes_read) /
                        static_cast<double>(total_message_read)
                 << " average message size";
    LOG(WARNING) << static_cast<double>(total_bytes_read) /
                        (timeout_ * 1024 * 1024)
                 << " MiB/s throughput";
    conn->GetLoop()->QueueInLoop(std::bind(&Client::quit, this));
  }
}

void HandleTimeout() {
  LOG(WARNING) << "stop";
  for (auto &session : sessions_) {
    session->Stop();
  }
}

private:
void quit() { loop_->QueueInLoop(std::bind(&EventLoop::Quit, loop_)); }

EventLoop *loop_;
EventLoopThreadPool thread_pool_;
int session_count_;
int timeout_;
std::vector<std::unique_ptr<Session>> sessions_;
std::string message_;
std::atomic_int32_t num_connected_;
std::unique_ptr<EpollTimer> timeout_timer_;

DISALLOW_COPY_AND_ASSIGN(Client);
};

void Session::onConnection(const TCPConnectionPtr &conn) {
  if (conn->Connected()) {
    conn->SetTCPNoDelay();
    conn->Send(owner_->message());
    owner_->OnConnect();
  } else {
    owner_->OnDisconnect(conn);
  }
}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  if (argc != 7) {
    fprintf(stderr, "Usage: client <host_ip> <port> <threads> <blocksize> ");
    fprintf(stderr, "<sessions> <time>\n");
  } else {
    LOG(INFO) << "pid = " << getpid()
              << ", tid = " << std::this_thread::get_id();

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int thread_count = atoi(argv[3]);
    int block_size = atoi(argv[4]);
    int session_count = atoi(argv[5]);
    int timeout = atoi(argv[6]);

    EventLoop loop;
    Client client(&loop, ip, port, block_size, session_count, timeout,
                  thread_count);
    loop.Loop();
  }
}
