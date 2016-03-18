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
                           ServiceControlClientOptions& options);

  // Override the destructor.
  ~ServiceControlClientImpl() override;

  // An async check call.
  void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) override;

  // A sync check call.
  ::google::protobuf::util::Status Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response)
      override;

  // An async report call.
  void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) override;

  // A sync report call.
  ::google::protobuf::util::Status Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response)
      override;

 private:
  // A flush callback for check.
  void CheckFlushCallback(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request);

  // A flush callback for report.
  void ReportFlushCallback(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request);

  // Gets next flush interval
  int GetNextFlushInterval();

  // Flushes out all items.
  google::protobuf::util::Status FlushAll();

  // Flushes out expired items.
  ::google::protobuf::util::Status Flush();

  // The transport object.
  std::shared_ptr<Transport> transport_;

  // The check aggregator object. Uses shared_ptr for check_aggregator_.
  // Transport::on_check_done() callback needs to call check_aggregator_
  // CacheResponse() function. The callback function needs to hold a ref_count
  // of check_aggregator_ to make sure it is not freed.
  std::shared_ptr<CheckAggregator> check_aggregator_;

  // The report aggregator object.
  std::unique_ptr<ReportAggregator> report_aggregator_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ServiceControlClientImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
