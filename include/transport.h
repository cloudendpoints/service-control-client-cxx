#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_TRANSPORT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_TRANSPORT_H_

#include <functional>
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {

// Non blocking transport interface to talk to Service Control Server.
class NonBlockingTransport {
 public:
  // Sends check_request protobuf to service control server.
  // check_response is valid after on_check_done() is called and status is OK.
  virtual void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      std::function<void(const ::google::protobuf::util::Status&)>
          on_check_done) = 0;

  // Sends report_request protobuf to service control server.
  // resport_response is valid after on_report_done() is called and status is
  // OK.
  virtual void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      std::function<void(const ::google::protobuf::util::Status&)>
          on_report_done) = 0;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
