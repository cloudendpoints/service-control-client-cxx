#include "gtest/gtest.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"

namespace google {
namespace service_control_client {
namespace {

// Unit tests to use CheckRequest protobuf message
TEST(ServiceControlProto, TestCheckRequest) {
  ::google::api::servicecontrol::v1::ReportRequest request;
  request.set_service_name("service-name");
  ASSERT_EQ("service-name", request.service_name());
}

} // namespace
} // namespace service_control_client
} // namespace google

