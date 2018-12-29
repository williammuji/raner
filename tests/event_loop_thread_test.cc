#include "raner/event_loop_thread.h"
#include "raner/event_loop.h"

#include <gtest/gtest.h>

namespace raner {
namespace {

std::thread::id main_thread_id;
std::thread::id new_thread_id;
EventLoop *g_main_loop = nullptr;

void new_thread_init(EventLoop *event_loop = nullptr) {
  new_thread_id = std::this_thread::get_id();
  EXPECT_NE(new_thread_id, main_thread_id);
}

void runInNewLoop() {
  EXPECT_EQ(new_thread_id, std::this_thread::get_id());
  EXPECT_NE(new_thread_id, main_thread_id);
}

void queueInNewLoop() {
  EXPECT_EQ(new_thread_id, std::this_thread::get_id());
  EXPECT_NE(new_thread_id, main_thread_id);
}

void quitMainLoop(EventLoop *main_loop) { main_loop->Quit(); }

TEST(EventLoopThreadTest, StartLoop) {
  EventLoop loop;

  main_thread_id = std::this_thread::get_id();
  g_main_loop = &loop;

  EventLoopThread new_thread(new_thread_init, "new_thread");
  EventLoop *new_loop = new_thread.StartLoop();
  EXPECT_NE(g_main_loop, new_loop);

  new_loop->RunInLoop(runInNewLoop);
  new_loop->RunInLoop(queueInNewLoop);
  new_loop->QueueInLoop(std::bind(quitMainLoop, &loop));

  loop.Loop();
}

}  // namespace
}  // namespace raner
