#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"
#include "include/aggregation_options.h"
#include "include/periodic_timer.h"
#include "include/transport.h"

namespace google {
namespace service_control_client {

// Defines the options to create an instance of ServiceControlClient interface.
struct ServiceControlClientOptions {
  // Default constructor with default values.
  ServiceControlClientOptions() {}

  // Constructor with specified option values.
  ServiceControlClientOptions(const CheckAggregationOptions& check_options,
                              const ReportAggregationOptions& report_options,
                              std::shared_ptr<MetricKindMap> metric_kinds)
      : check_options(check_options),
        report_options(report_options),
        metric_kinds(metric_kinds) {}

  // Check aggregation options.
  CheckAggregationOptions check_options;

  // Report aggregation options.
  ReportAggregationOptions report_options;

  // Metric map to map metric name to metric kind.  This info can be
  // extracted from Metric definitions from service config.
  // If a metric is not specified in this map, use DELTA as its kind.
  std::shared_ptr<MetricKindMap> metric_kinds;

  // Transport object is used to send request to service control server.
  // It can be implemented many ways based on the environments.
  // A GRPC transport can be created by calling
  // CreateGrpcTransport() defined in transport.h
  // If not provided, the GRPC transport will be used.
  std::unique_ptr<Transport> transport;

  // This is only used when transport is NOT provided. The library will
  // use this GRPC server name to create a GRPC transport.
  std::string service_control_grpc_server;

  // The object to create a periodic timer for the library to flush out
  // expired items. If not provided, the library will create a thread
  // based periodic timer.
  std::unique_ptr<PeriodicTimer> periodic_timer;
};

// Service control client interface. It is thread safe.
// Here are some usage examples:
//
// 1) Simplest way; uses all default options and calls blocking functions.
//
//    // A GRPC transport will be used.
//    // A thread based PeriodicTimer will be used to call Flush().
//    ServiceControlClientOptions options;
//    options.service_control_grpc_server = "service-control-grpc-server";
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
//    // Constructs a CheckRequest protobuf check_request;
//    CheckResponse check_response;
//    Status status = client->BlockingCheck(check_request, &check_response);
//    if (status.ok()) {
//       // Inspects check_response;
//    }
//
//    // Constructs a ReportRequest protobuf report_request;
//    ReportResponse report_response;
//    Status status = client->BlockingReport(report_request, &report_response);
//    if (status.ok()) {
//       // Inspects report_response;
//    }
//
// 2) Advance way; uses custom transport, calls non_blocking methods.
//
//    ServiceControlClientOptions options;
//    // Creates a transport object.
//    options.transport =
//         std::move(your-custom-transport);
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
//    // Constructs a CheckRequest protobuf check_request;
//    // Calls Check in a non blocking way by providing a callback.
//    CheckResponse check_response;
//    client->Check(check_request, &check_response,
//         [](const Status& status) {
//             if (status.ok()) {
//                 // Inspects check_response;
//             }
//         });
//
//    // Constructs a ReportRequest protobuf report_request;
//    // Calls Report in a non blocking way by providing a callback.
//    ReportResponse report_response;
//    client->Report(report_request, &report_response,
//         [](const Status& status) {
//             if (status.ok()) {
//                 // Inspects report_response;
//             }
//         });
//
class ServiceControlClient {
 public:
  using DoneCallback =
      std::function<void(const ::google::protobuf::util::Status&)>;

  // Destructor
  virtual ~ServiceControlClient() {}

  // Checks quota, billing status, service activation status etc. with cache
  // support.
  //
  // High importance operations are sent directly to the server without any
  // caching. Low importance operations may be cached and accumulated before
  // sending to the server. The corresponding cached response is returned for
  // a low importance operation if it exists in the cache. The operations
  // accumulated in the cache are flushed to the server when the cache runs
  // out of capacity or when determined by the parameters in
  // check_aggregation_options in the constructor.
  //
  // For more details on high/low importance operations, see Importance
  // defined in //google/api/servicecontrol/v1/operation.proto.

  // The non-blocking call.
  // on_check_done is called with the check status after cached
  // check_response is returned in case of cache hit, otherwise called after
  // check_response is returned from the Controller service.
  //
  // check_response must be alive until on_check_done is called.
  virtual void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) = 0;

  // The blocking call.
  // If it is a cache miss, this function will call remote Chemist server, wait
  // for its response.
  virtual ::google::protobuf::util::Status BlockingCheck(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response) = 0;

  // Reports operations to the Controller service for billing, logging,
  // monitoring, etc.
  // High importance operations are sent directly to the server without any
  // caching. Low importance operations may be cached and accumulated before
  // sending to the server. The operations accumulated in the cache are flushed
  // to the server when the cache runs out of capacity or when determined by
  // the parameters in report_aggregation_options in the constructor.
  //
  // For more details on high/low importance operations, see Importance
  // defined in //google/api/servicecontrol/v1/operation.proto.

  // This is non-blocking call. on_report_done is always called when the
  // report request is finished.
  virtual void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) = 0;

  // This is the blocking call.  If Report is cached, the function will return
  // after the data is saved in the cache. If report is not cached (High
  // important operations), this function will send the data to remote server,
  // and wait for its response.
  virtual ::google::protobuf::util::Status BlockingReport(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response) = 0;

  // Clears all cache items. Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll() = 0;
};

// Creates a ServiceControlClient object.
std::unique_ptr<ServiceControlClient> CreateServiceControlClient(
    const std::string& service_name, ServiceControlClientOptions& options);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
