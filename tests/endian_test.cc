#include "raner/endian.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"

namespace raner {
namespace {

#pragma GCC diagnostic ignored "-Wunused-const-variable"

const uint64_t kInitialNumber{0x0123456789abcdef};
const uint64_t k64Value{kInitialNumber};
const uint32_t k32Value{0x01234567};
const uint16_t k16Value{0x0123};
const int kNumValuesToTest = 1000000;
const int kRandomSeed = 12345;

#ifdef RANER_IS_BIG_ENDIAN
const uint64_t kInitialInNetworkOrder{kInitialNumber};
const uint64_t k64ValueLE{0xefcdab8967452301};
const uint32_t k32ValueLE{0x67452301};
const uint16_t k16ValueLE{0x2301};
const uint8_t k8ValueLE{k8Value};
const uint64_t k64IValueLE{0xefcdab89674523a1};
const uint32_t k32IValueLE{0x67452391};
const uint16_t k16IValueLE{0x85ff};
const uint8_t k8IValueLE{0xff};
const uint64_t kDoubleValueLE{0x6e861bf0f9210940};
const uint32_t kFloatValueLE{0xd00f4940};
const uint8_t kBoolValueLE{0x1};

const uint64_t k64ValueBE{kInitialNumber};
const uint32_t k32ValueBE{k32Value};
const uint16_t k16ValueBE{k16Value};
const uint8_t k8ValueBE{k8Value};
const uint64_t k64IValueBE{0xa123456789abcdef};
const uint32_t k32IValueBE{0x91234567};
const uint16_t k16IValueBE{0xff85};
const uint8_t k8IValueBE{0xff};
const uint64_t kDoubleValueBE{0x400921f9f01b866e};
const uint32_t kFloatValueBE{0x40490fd0};
const uint8_t kBoolValueBE{0x1};
#elif defined RANER_IS_LITTLE_ENDIAN
const uint64_t kInitialInNetworkOrder{0xefcdab8967452301};
const uint64_t k64ValueLE{kInitialNumber};
const uint32_t k32ValueLE{k32Value};
const uint16_t k16ValueLE{k16Value};

const uint64_t k64ValueBE{0xefcdab8967452301};
const uint32_t k32ValueBE{0x67452301};
const uint16_t k16ValueBE{0x2301};
#endif

template <typename T>
std::vector<T> GenerateAllValuesForType() {
  std::vector<T> result;
  T next = std::numeric_limits<T>::min();
  while (true) {
    result.push_back(next);
    if (next == std::numeric_limits<T>::max()) {
      return result;
    }
    ++next;
  }
}

template <typename T>
std::vector<T> GenerateRandomIntegers(size_t numValuesToTest) {
  std::vector<T> result;
  std::mt19937_64 rng(kRandomSeed);
  for (size_t i = 0; i < numValuesToTest; ++i) {
    result.push_back(static_cast<T>(rng()));
  }
  return result;
}

void ManualByteSwap(char *bytes, int length) {
  if (length == 1) return;

  EXPECT_EQ(0, length % 2);
  for (int i = 0; i < length / 2; ++i) {
    int j = (length - 1) - i;
    using std::swap;
    swap(bytes[i], bytes[j]);
  }
}

template <typename T>
inline T UnalignedLoad(const char *p) {
  static_assert(
      sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
      "Unexpected type size");
  return *reinterpret_cast<const T *>(p);
}

template <typename T, typename ByteSwapper>
static void GBSwapHelper(const std::vector<T> &host_values_to_test,
                         const ByteSwapper &byte_swapper) {
  // Test byte_swapper against a manual byte swap.
  for (typename std::vector<T>::const_iterator it = host_values_to_test.begin();
       it != host_values_to_test.end(); ++it) {
    T host_value = *it;

    char actual_value[sizeof(host_value)];
    memcpy(actual_value, &host_value, sizeof(host_value));
    byte_swapper(actual_value);

    char expected_value[sizeof(host_value)];
    memcpy(expected_value, &host_value, sizeof(host_value));
    ManualByteSwap(expected_value, sizeof(host_value));

    ASSERT_EQ(0, memcmp(actual_value, expected_value, sizeof(host_value)))
        << "Swap output for 0x" << std::hex << host_value << " does not match. "
        << "Expected: 0x" << UnalignedLoad<T>(expected_value) << "; "
        << "actual: 0x" << UnalignedLoad<T>(actual_value);
  }
}

void Swap16(char *bytes) {
  uint16_t num = gbswap_16(*reinterpret_cast<const uint16_t *>(bytes));
  memcpy(bytes, &num, sizeof(num));
}

void Swap32(char *bytes) {
  uint32_t num = gbswap_32(*reinterpret_cast<const uint32_t *>(bytes));
  memcpy(bytes, &num, sizeof(num));
}

void Swap64(char *bytes) {
  uint64_t num = gbswap_64(*reinterpret_cast<const uint64_t *>(bytes));
  memcpy(bytes, &num, sizeof(num));
}

TEST(EndianessTest, Uint16) {
  GBSwapHelper(GenerateAllValuesForType<uint16_t>(), &Swap16);
}

TEST(EndianessTest, Uint32) {
  GBSwapHelper(GenerateRandomIntegers<uint32_t>(kNumValuesToTest), &Swap32);
}

TEST(EndianessTest, Uint64) {
  GBSwapHelper(GenerateRandomIntegers<uint64_t>(kNumValuesToTest), &Swap64);
}

TEST(EndianessTest, ghtonll_gntohll) {
  // Test that raner::ghtonl compiles correctly
  uint32_t test = 0x01234567;
  EXPECT_EQ(raner::gntohl(raner::ghtonl(test)), test);

  uint64_t comp = raner::ghtonll(kInitialNumber);
  EXPECT_EQ(comp, kInitialInNetworkOrder);
  comp = raner::gntohll(kInitialInNetworkOrder);
  EXPECT_EQ(comp, kInitialNumber);

  // Test that htonll and ntohll are each others' inverse functions on a
  // somewhat assorted batch of numbers. 37 is chosen to not be anything
  // particularly nice base 2.
  uint64_t value = 1;
  for (int i = 0; i < 100; ++i) {
    comp = raner::ghtonll(raner::gntohll(value));
    EXPECT_EQ(value, comp);
    comp = raner::gntohll(raner::ghtonll(value));
    EXPECT_EQ(value, comp);
    value *= 37;
  }
}

#pragma GCC diagnostic warning "-Wunused-const-variable"

}  // namespace
}  // namespace raner
