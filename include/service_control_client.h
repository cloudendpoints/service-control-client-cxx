#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"
#include "include/aggregator_options.h"
#include "include/plugable_interfaces.h"

namespace google {
namespace service_control_client {

// Defines the options to create an instance of ServiceControlClient interface.
struct ServiceControlClientOptions {
  // Default constructor with default values.
  ServiceControlClientOptions() : call_flush_manually(false) {}

  // Constructor with specified option values.
  ServiceControlClientOptions(const CheckAggregationOptions& check_options,
                              const ReportAggregationOptions& report_options,
                              std::shared_ptr<MetricKindMap> metric_kinds,
                              bool call_flush_manually)
      : check_options(check_options),
        report_options(report_options),
        metric_kinds(metric_kinds),
        call_flush_manually(call_flush_manually) {}

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
  // Either of non_blocking_transport or blocking_transport can
  // be provided. If both non_blocking and blocking transport are provided,
  // the library will use non_blocking transport for non blocking methods
  // and the blocking transport for blocking methods.
  // A GRPC non blocking transport can be created by calling
  // CreateGrpcNonBlockingTransport() defined in plugtable_interfaces.h
  // A GRPC blocking transport can be created by calling
  // CreateGrpcBlockingTransport() defined in plugtable_interfaces.h
  // If neither transport is provided, the GRPC non blocking transport will
  // be used for non blocking methods and the GRPC blocking transport will
  // be used for blocking methods.
  std::unique_ptr<NonBlockingTransport> non_blocking_transport;
  std::unique_ptr<BlockingTransport> blocking_transport;

  // This is only used when neither non_blocking_transport or blocking_transport
  // is provided. The library will use this GRPC server name to create a GRPC
  // non
  // blocking transport.
  std::string service_control_grpc_server;

  // This object is required if a blocking_transport is provided, but not the
  // non_blocking_transport. It will be used to run a blocking transport task
  // in a separate thread. A thread based TaskRunner object can be created by
  // calling CreateThreadBasedTaskRunner() defined in plugtable_interfaces.h.
  // If not provided, the thread based TaskRunner will be used if needed.
  std::unique_ptr<TaskRunner> task_runner;

  // Determines ways to call Flush() periodically.
  // If false, PeriodicTimer will be used internally to call Flush() with
  // desired interval. If priodic_timer is not provided, a default thread
  // based PeriodicTimer will be used.
  // If true, the caller has to call Flush() with interval from
  // GetNextFlushInterval().
  // By default, call_flush_manually is false.
  bool call_flush_manually;

  // This object is required when call_flush_manually is false.
  // The periodic_timer object will be used to call
  // ServiceControlClient->Flush() function as following:
  //
  //   periodic_timer->Start(
  //          ServiceControlClient->GetNextFlushInterval(),
  //          [ServiceControlClient]() { ServiceControlClient->Flush(); },
  //          &timer_id);
  //
  // A thread based PeriodicTimer can be created by calling
  // CreateThreadBasedPeriodicTimer defined in plugtable_interfaces.h.
  // If not provided,  the thread based PeriodicTimer will be used if needed.
  std::unique_ptr<PeriodicTimer> periodic_timer;
};

// Service control client interface. It is thread safe.
// Here are some usage examples:
//
// 1) Simplest way; uses all default options and calls blocking functions.
//
//    // A GRPC non blocking transport will be used.
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
// 2) Most popular way; uses custom non_blocking_transport, calls
//    non_blocking methods and calls Flush manually.
//
//    ServiceControlClientOptions options;
//    // Creates a non_blocking_transport object.
//    options.non_blocking_transport =
//         std::move(your-custom-non-blocking-transport);
//    // Calls Flush() manually.
//    options.calls_flush_manually = true;
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
//    // Call client->Flush() periodically with interval from
//    // client->GetNextFlushInterval()
//    // client->Flush();
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
  // caching. Low importance operations may be cached before sending to the
  // server. The corresponding cached response is returned for a low importance
  // operation if it exists in the cache. The operations accumulated in the
  // cache are flushed to the server when the cache runs out of capacity or when
  // determined by the parameters in check_aggregation_options in the
  // constructor.
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
  // caching. Low importance operations may be cached before sending to the
  // server. The operations accumulated in the cache are flushed to the server
  // when the cache runs out of capacity or when determined by the parameters in
  // report_aggregation_options in the constructor.
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

  // When the next Flush() should be called.
  // Returns in ms from now, or -1 for never
  virtual int GetNextFlushInterval() = 0;

  // Flushes cached items longer than flush_interval.
  // Called at time specified by GetNextFlushInterval().
  virtual ::google::protobuf::util::Status Flush() = 0;

  // Clears all cache items. Usually called at destructor.
  virtual ::google::protobuf::util::Status FlushAll() = 0;
};

// Creates a ServiceControlClient object.
std::unique_ptr<ServiceControlClient> CreateServiceControlClient(
    const std::string& service_name, ServiceControlClientOptions& options);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_H_
