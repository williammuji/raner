#include "raner/byte_buffer.h"

#include <gtest/gtest.h>

namespace raner {
namespace {

TEST(ByteBufferTest, WriteSkip) {
  ByteBuffer buf;
  EXPECT_EQ(buf.ReadableBytes(), 0);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize);

  const std::string str(200, 'x');
  buf.Write(str);
  EXPECT_EQ(buf.ReadableBytes(), str.size());
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - str.size());

  const std::string str2 = buf.SkipAsString(50);
  EXPECT_EQ(str2.size(), 50);
  EXPECT_EQ(buf.ReadableBytes(), str.size() - str2.size());
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - str.size());
  EXPECT_EQ(str2, std::string(50, 'x'));

  buf.Write(str);
  EXPECT_EQ(buf.ReadableBytes(), 2 * str.size() - str2.size());
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 2 * str.size());

  const std::string str3 = buf.SkipAllAsString();
  EXPECT_EQ(str3.size(), 350);
  EXPECT_EQ(buf.ReadableBytes(), 0);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize);
  EXPECT_EQ(str3, std::string(350, 'x'));
}

TEST(ByteBufferTest, Grow) {
  ByteBuffer buf;
  buf.Write(std::string(400, 'y'));
  EXPECT_EQ(buf.ReadableBytes(), 400);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 400);

  buf.SkipReadBytes(50);
  EXPECT_EQ(buf.ReadableBytes(), 350);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 400);

  buf.Write(std::string(1000, 'z'));
  EXPECT_EQ(buf.ReadableBytes(), 1350);
  EXPECT_EQ(buf.WritableBytes(), 0);

  buf.SkipAll();
  EXPECT_EQ(buf.ReadableBytes(), 0);
  EXPECT_EQ(buf.WritableBytes(), 1400);
}

TEST(ByteBufferTest, InsideGrow) {
  ByteBuffer buf;
  buf.Write(std::string(800, 'y'));
  EXPECT_EQ(buf.ReadableBytes(), 800);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 800);

  buf.SkipReadBytes(500);
  EXPECT_EQ(buf.ReadableBytes(), 300) << buf.ReadableBytes();
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 800);

  buf.Write(std::string(300, 'z'));
  EXPECT_EQ(buf.ReadableBytes(), 600);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 600);
}

TEST(ByteBufferTest, Shrink) {
  ByteBuffer buf;
  buf.Write(std::string(2000, 'y'));
  EXPECT_EQ(buf.ReadableBytes(), 2000);
  EXPECT_EQ(buf.WritableBytes(), 0);

  buf.SkipReadBytes(1500);
  EXPECT_EQ(buf.ReadableBytes(), 500);
  EXPECT_EQ(buf.WritableBytes(), 0);

  buf.Shrink();
  EXPECT_EQ(buf.ReadableBytes(), 500);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize - 500);
  EXPECT_EQ(buf.SkipAllAsString(), std::string(500, 'y'));
}

TEST(ByteBufferTest, ReadInt) {
  ByteBuffer buf;
  buf.Write("HTTP");

  EXPECT_EQ(buf.ReadableBytes(), 4);
  EXPECT_EQ(buf.PeekInt8(), 'H');
  int top16 = buf.PeekInt16();
  EXPECT_EQ(top16, 'H' * 256 + 'T');
  EXPECT_EQ(buf.PeekInt32(), top16 * 65536 + 'T' * 256 + 'P');

  EXPECT_EQ(buf.ReadInt8(), 'H');
  EXPECT_EQ(buf.ReadInt16(), 'T' * 256 + 'T');
  EXPECT_EQ(buf.ReadInt8(), 'P');
  EXPECT_EQ(buf.ReadableBytes(), 0);
  EXPECT_EQ(buf.WritableBytes(), ByteBuffer::kInitialSize);

  buf.WriteInt8(-1);
  buf.WriteInt16(-2);
  buf.WriteInt32(-3);
  EXPECT_EQ(buf.ReadableBytes(), 7);
  EXPECT_EQ(buf.ReadInt8(), -1);
  EXPECT_EQ(buf.ReadInt16(), -2);
  EXPECT_EQ(buf.ReadInt32(), -3);
}

TEST(ByteBufferTest, FindEOL) {
  ByteBuffer buf;
  buf.Write(std::string(100000, 'x'));
  const char *null = nullptr;
  EXPECT_EQ(buf.FindEOL(), null);
  EXPECT_EQ(buf.FindEOL(buf.BeginRead() + 90000), null);
}

void output(ByteBuffer &&buf, const void *inner) {
  ByteBuffer newbuf(std::move(buf));
  // printf("New ByteBuffer at %p, inner %p\n", &newbuf, newbuf.peek());
  EXPECT_EQ(inner, newbuf.BeginRead());
}

// NOTE: This test fails in g++ 4.4, passes in g++ 4.6.
TEST(ByteBufferTest, Move) {
  ByteBuffer buf;
  buf.Write("raner", 5);
  const void *inner = buf.BeginRead();
  // printf("ByteBuffer at %p, inner %p\n", &buf, inner);
  output(std::move(buf), inner);
}

}  // namespace
}  // namespace raner

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
