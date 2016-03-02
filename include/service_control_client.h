#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_

#include <functional>
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {

// Service control client interface. It is thread safe.
class ServiceControlClient {
 public:
  using DoneCallback =
      std::function<void(const ::google::protobuf::util::Status&)>;

  // Checks quota, billing status, service activation status etc. with cache
  // support.
  //
  // High importance operations are sent directly to the server without any
  // caching. Low importance operations may be cached before sending to the
  // server. The corresponding cached response is returned for a low importance
  // operation if it exists in the cache. The operations accumulated in the
  // cache are flushed to the server when the cache runs out of capacity or when
  // determined by the parameters in check_aggregation_options in the
  // constructor.
  //
  // For more details on high/low importance operations, see Importance
  // defined // in //google/api/servicecontrol/v1/operation.proto.

  // The non-blocking call.
  // on_check_done is called with the check status after cached
  // check_response is returned in case of cache hit, otherwise called after
  // check_response is returned from the Controller service.
  //
  // Does not take ownership of response, which must be alive until
  // on_check_done is called.
  virtual void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) = 0;

  // The blocking call.
  // If it is a cache miss, this function will call remote Chemist server, wait
  // for its
  // response.
  virtual ::google::protobuf::util::Status Blocking_Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response) = 0;

  // Reports operations to the Controller service for billing, logging,
  // monitoring, etc.   //
  // High importance operations are sent directly to the server without any
  // caching. Low importance operations may be cached before sending to the
  // server. The operations accumulated in the cache are flushed to the server
  // when the cache runs out of capacity or when determined by the parameters in
  // report_aggregation_options in the constructor.
  //
  // For more details on high/low importance operations, see Importance
  // defined // in //google/api/servicecontrol/v1/operation.proto.

  // This is non-blocking call. on_report_done is always called when the
  // report request is finished.
  virtual void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) = 0;

  // This is the blocking call.  If Report is cached, the function will return
  // after the data
  // is saved in the cache.   If report is not cached (High important
  // operations), this
  // function will send the data to remote Chemist server, and wait for its
  // response.
  virtual ::google::protobuf::util::Status Blocking_Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response) = 0;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
