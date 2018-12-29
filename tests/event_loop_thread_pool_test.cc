#include "raner/event_loop_thread_pool.h"
#include "raner/event_loop.h"

#include <gtest/gtest.h>

namespace raner {
namespace {

TEST(EventLoopThreadPoolTest, GetNextLoop) {
  EventLoop loop;

  {
    EventLoopThreadPool pool(&loop, "single");
    pool.SetThreadNum(0);
    pool.Start();
    EXPECT_EQ(pool.GetNextLoop(), &loop);
    EXPECT_EQ(pool.GetNextLoop(), &loop);
    EXPECT_EQ(pool.GetNextLoop(), &loop);
  }

  {
    EventLoopThreadPool pool(&loop, "another");
    pool.SetThreadNum(1);
    pool.Start();
    EventLoop *next_loop = pool.GetNextLoop();
    EXPECT_NE(next_loop, &loop);
    EXPECT_EQ(next_loop, pool.GetNextLoop());
    EXPECT_EQ(next_loop, pool.GetNextLoop());
  }

  {
    EventLoopThreadPool pool(&loop, "three");
    pool.SetThreadNum(3);
    pool.Start();
    EventLoop *next_loop = pool.GetNextLoop();
    EXPECT_NE(next_loop, &loop);
    EXPECT_NE(next_loop, pool.GetNextLoop());
    EXPECT_NE(next_loop, pool.GetNextLoop());
    EXPECT_EQ(next_loop, pool.GetNextLoop());
  }

  loop.QueueInLoop(std::bind(&EventLoop::Quit, &loop));
  loop.Loop();
}

}  // namespace
}  // namespace raner
