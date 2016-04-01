#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"
// To make it easier for other packages to include this module as their
// submodule, following include rules have to be followed:
// A public exposed header can only include local headers in the same folder.
// When including, not to put folder name in the include, just the file name.
#include "aggregation_options.h"
#include "periodic_timer.h"
#include "request_context.h"
#include "transport.h"

namespace google {
namespace service_control_client {

// Defines the options to create an instance of ServiceControlClient interface.
struct ServiceControlClientOptions {
  // Default constructor with default values.
  ServiceControlClientOptions() {}

  // Constructor with specified option values.
  ServiceControlClientOptions(const CheckAggregationOptions& check_options,
                              const ReportAggregationOptions& report_options)
      : check_options(check_options), report_options(report_options) {}

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
  std::shared_ptr<Transport> transport;

  // This is only used when transport is NOT provided. The library will
  // use this GRPC server name to create a GRPC transport.
  std::string service_control_grpc_server;

  // The object to create a periodic timer for the library to flush out
  // expired items. If not provided, the library will create a thread
  // based periodic timer.
  std::shared_ptr<PeriodicTimer> periodic_timer;
};

// Service control client interface. It is thread safe.
// Here are some usage examples:
//
// 1) Creates an instance of ServiceControlClient.
//
// 1.1) Uses all default options.
//
//    ServiceControlClientOptions options;
//    // Creates your custom transport object.
//    options.service_control_grpc_server = "service-control-grpc-server";
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
// 1.2) Uses custom aggregation options.
//
//    ServiceControlClientOptions options(
//        CheckAggregationOptions(500000 /* cache_entries */,
//                                2000   /* flush interval in ms */,
//                                5000   /* response expiration in ms*/),
//        ReportAggregationOptions(800000 /* cache_entries */,
//                                 2000   /* flush interval in ms */));
//    // Uses GRPC transport with this grpc_server.
//    options.service_control_grpc_server = "service-control-grpc-server";
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
// 1.3) Uses a custom transport.
//
//    ServiceControlClientOptions options;
//    // Creates your custom transport object.
//    options.transport = your-custom-transport;
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
// 1.4) Uses a custom periodic timer.
//
//    ServiceControlClientOptions options;
//    // Creates your custom periodic timer.
//    options.periodic_timer = your-custom-periodic-timer;
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
// 1.5) Disables caching and aggregation
//
//    ServiceControlClientOptions options(
//        CheckAggregationOptions(0, 0, 0),
//        ReportAggregationOptions(0, 0));
//    std::unique_ptr<ServiceControlClient> client = std::move(
//       CreateServiceControlClient("your-service-name", options));
//
// 2) Makes sync vs async calls.
//
// 2.1) Makes sync calls:
//
//    // Constructs a CheckRequest protobuf check_request;
//    CheckResponse check_response;
//    Status status = client->Check(ctx, check_request, &check_response);
//    if (status.ok()) {
//       // Inspects check_response;
//    }
//
//    // Constructs a ReportRequest protobuf report_request;
//    ReportResponse report_response;
//    Status status = client->Report(ctx, report_request, &report_response);
//    if (status.ok()) {
//       // Inspects report_response;
//    }
//
// 2.2) Makes async calls:
//
//    // Constructs a CheckRequest protobuf check_request;
//    // Calls async Check by providing a callback.
//    CheckResponse check_response;
//    client->Check(ctx, check_request, &check_response,
//         [](const Status& status) {
//             if (status.ok()) {
//                 // Inspects check_response;
//             }
//         });
//
//    // Constructs a ReportRequest protobuf report_request;
//    // Calls async Report by providing a callback.
//    ReportResponse report_response;
//    client->Report(ctx, report_request, &report_response,
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

  // The async call.
  // on_check_done is called with the check status after cached
  // check_response is returned in case of cache hit, otherwise called after
  // check_response is returned from the Controller service.
  //
  // check_response must be alive until on_check_done is called.
  virtual void Check(
      RequestContext* ctx,
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) = 0;

  // The sync call.
  // If it is a cache miss, this function will call remote Chemist server, wait
  // for its response.
  virtual ::google::protobuf::util::Status Check(
      RequestContext* ctx,
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

  // This is async call. on_report_done is always called when the
  // report request is finished.
  virtual void Report(
      RequestContext* ctx,
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) = 0;

  // This is sync call.  If Report is cached, the function will return
  // after the data is saved in the cache. If report is not cached (High
  // important operations), this function will send the data to remote server,
  // and wait for its response.
  virtual ::google::protobuf::util::Status Report(
      RequestContext* ctx,
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response) = 0;
};

// Creates a ServiceControlClient object.
std::unique_ptr<ServiceControlClient> CreateServiceControlClient(
    const std::string& service_name, ServiceControlClientOptions& options);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_