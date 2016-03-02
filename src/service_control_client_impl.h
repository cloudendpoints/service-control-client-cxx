#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "include/service_control_client.h"

namespace google {
namespace service_control_client {

// Caches/Batches/aggregates report requests and sends them to the server.
// Thread safe.
class ServiceControlClientImpl : public ServiceControlClient {
 public:
  // Constructor.
  // Does not take ownership of metric_kinds and controller, which must outlive
  // this instance.
  ServiceControlClientImpl(const std::string& service_name,
                           const CheckAggregationOptions& check_options,
                           const ReportAggregationOptions& report_options,
                           std::shared_ptr<MetricKindMap> metric_kind);

  ~ServiceControlClientImpl() override;

  void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) override;

  ::google::protobuf::util::Status Blocking_Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response)
      override;

  void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) override;

  ::google::protobuf::util::Status Blocking_Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response)
      override;

 private:
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
