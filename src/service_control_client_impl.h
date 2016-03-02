#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_

#include "include/service_control_client.h"
#include "src/aggregator_interface.h"
#include "utils/google_macros.h"

namespace google {
namespace service_control_client {

// ServiceControlClient implementation class.
// Thread safe.
class ServiceControlClientImpl : public ServiceControlClient {
 public:
  // Constructor.
  ServiceControlClientImpl(const std::string& service_name,
                           const CheckAggregationOptions& check_options,
                           const ReportAggregationOptions& report_options,
                           std::shared_ptr<MetricKindMap> metric_kind);

  // Override the destructor.
  ~ServiceControlClientImpl() override;

  // Sets NonBlockingTransport object.
  void SetNonBlockingTransport(
      std::unique_ptr<NonBlockingTransport> non_blocking_transport);

  // A nonblocking Check call.
  void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) override;

  // A blocking Check call.
  ::google::protobuf::util::Status Blocking_Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response)
      override;

  // A nonblocking report call.
  void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) override;

  // A blocking report call.
  ::google::protobuf::util::Status Blocking_Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response)
      override;

  // Gets next flush interval
  int GetNextFlushInterval() override;

  // Flushes out expired items.
  ::google::protobuf::util::Status Flush() override;

  // Flushes out all items.
  google::protobuf::util::Status FlushAll() override;

 private:
  // A flush callback for check.
  void CheckFlushCallback(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request);

  // A flush callback for report.
  void ReportFlushCallback(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request);

  // Non blocking transport object.
  std::unique_ptr<NonBlockingTransport> non_blocking_transport_;

  // The check aggregator object.
  // Uses shared_ptr so on_done() callback can hold a ref_count.
  std::shared_ptr<CheckAggregator> check_aggregator_;

  // The report aggregator object.
  std::unique_ptr<ReportAggregator> report_aggregator_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ServiceControlClientImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
