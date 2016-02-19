#include "md5.h"
#include "gtest/gtest.h"

namespace google {
namespace service_control_client {
namespace {

// Unit tests to use CheckRequest protobuf message

TEST(MD5Test, TestPriableGigest) {
  static const char data[] = "Test Data";
  ASSERT_EQ("0a22b2ac9d829ff3605d81d5ae5e9d16",
            MD5().Update(data, sizeof(data)).PrintableDigest());
}

TEST(MD5Test, TestGigestEqual) {
  static const char data1[] = "Test Data1";
  static const char data2[] = "Test Data2";
  auto d1 = MD5()(data1, sizeof(data1));
  auto d11 = MD5()(data1, sizeof(data1));
  auto d2 = MD5()(data2, sizeof(data2));
  ASSERT_EQ(d11, d1);
  ASSERT_NE(d1, d2);
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
