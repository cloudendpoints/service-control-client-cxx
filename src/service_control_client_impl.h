#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_

#include "include/service_control_client.h"
#include "src/aggregator_interface.h"
#include "utils/google_macros.h"

#include <atomic>

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

  // A check call with per_request transport.
  void Check(
      CheckTransport* check_transport,
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) override;

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

  ::google::protobuf::util::Status GetStatistics(
      Statistics* stat) const override;
  // A report call with per_request transport.
  void Report(
      ReportTransport* report_transport,
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) override;

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

  // The actual check logic code.
  void InternalCheck(
      CheckTransport* check_transport,
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done);

  // The actual report logic code.
  void InternalReport(
      ReportTransport* report_transport,
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done);

  // The check transport object.
  std::shared_ptr<CheckTransport> check_transport_;
  // The report transport object.
  std::shared_ptr<ReportTransport> report_transport_;

  // The Timer object.
  std::shared_ptr<PeriodicTimer::Timer> flush_timer_;

  // Atomic object to deal with multi-threads situation.
  std::atomic_int_fast64_t total_called_checks_;
  std::atomic_int_fast64_t send_checks_by_flush_;
  std::atomic_int_fast64_t send_checks_in_flight_;
  std::atomic_int_fast64_t total_called_reports_;
  std::atomic_int_fast64_t send_reports_by_flush_;
  std::atomic_int_fast64_t send_reports_in_flight_;
  std::atomic_int_fast64_t send_report_operations_;

  // The check aggregator object. Uses shared_ptr for check_aggregator_.
  // Transport::on_check_done() callback needs to call check_aggregator_
  // CacheResponse() function. The callback function needs to hold a ref_count
  // of check_aggregator_ to make sure it is not freed.
  std::shared_ptr<CheckAggregator> check_aggregator_;

  // The report aggregator object. report_aggregator_ has to be shared_ptr since
  // it will be passed to flush_timer callback.
  std::shared_ptr<ReportAggregator> report_aggregator_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ServiceControlClientImpl);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_H_
