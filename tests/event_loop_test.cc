#include "raner/event_loop.h"

#include <gtest/gtest.h>

namespace raner {
namespace {

std::thread::id main_thread_id;
std::thread::id new_thread_id;
EventLoop *g_main_loop = nullptr;

void runInMainLoop(EventLoop *main_loop) {
  EXPECT_EQ(main_thread_id, std::this_thread::get_id());
  EXPECT_NE(new_thread_id, std::this_thread::get_id());

  EXPECT_EQ(g_main_loop, main_loop);
  EXPECT_EQ(g_main_loop, EventLoop::GetEventLoopOfCurrentThread());
}

void queueInMainLoop(EventLoop *main_loop) {
  EXPECT_EQ(main_thread_id, std::this_thread::get_id());
  EXPECT_NE(new_thread_id, std::this_thread::get_id());

  EXPECT_EQ(g_main_loop, main_loop);
  EXPECT_EQ(g_main_loop, EventLoop::GetEventLoopOfCurrentThread());
}

void threadFunc(EventLoop *main_loop) {
  EXPECT_EQ(EventLoop::GetEventLoopOfCurrentThread(), nullptr);
  EventLoop loop;
  EXPECT_EQ(EventLoop::GetEventLoopOfCurrentThread(), &loop);

  new_thread_id = std::this_thread::get_id();
  EXPECT_NE(new_thread_id, main_thread_id);

  main_loop->RunInLoop(std::bind(runInMainLoop, main_loop));
  main_loop->QueueInLoop(std::bind(queueInMainLoop, main_loop));
  main_loop->QueueInLoop(std::bind(&EventLoop::Quit, main_loop));
}

TEST(EventLoopTest, GetCurrentThreadEventLoop) {
  EXPECT_EQ(EventLoop::GetEventLoopOfCurrentThread(), nullptr);
  EventLoop loop;
  EXPECT_EQ(EventLoop::GetEventLoopOfCurrentThread(), &loop);

  main_thread_id = std::this_thread::get_id();
  g_main_loop = &loop;

  std::thread new_thread(threadFunc, &loop);
  loop.Loop();
  new_thread.join();
}

}  // namespace
}  // namespace raner
