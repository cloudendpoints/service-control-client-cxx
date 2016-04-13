#include "src/service_control_client_impl.h"

#include "google/protobuf/stubs/logging.h"
#include "include/periodic_timer.h"
#include "utils/thread.h"

using std::string;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {

ServiceControlClientImpl::ServiceControlClientImpl(
    const string& service_name, ServiceControlClientOptions& options) {
  check_aggregator_ = std::move(CreateCheckAggregator(
      service_name, options.check_options, options.metric_kinds));
  report_aggregator_ = std::move(CreateReportAggregator(
      service_name, options.report_options, options.metric_kinds));

  check_transport_ = options.check_transport;
  report_transport_ = options.report_transport;

  total_called_checks_ = 0;
  send_checks_by_flush_ = 0;
  send_checks_in_flight_ = 0;
  total_called_reports_ = 0;
  send_reports_by_flush_ = 0;
  send_reports_in_flight_ = 0;
  send_report_operations_ = 0;

  check_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::CheckFlushCallback, this,
                std::placeholders::_1));
  report_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::ReportFlushCallback, this,
                std::placeholders::_1));

  int flush_interval = GetNextFlushInterval();
  if (options.periodic_timer && flush_interval > 0) {
    // Class members cannot be captured in lambda. We need to make a copy to
    // support C++11.
    std::shared_ptr<CheckAggregator> check_aggregator_copy = check_aggregator_;
    std::shared_ptr<ReportAggregator> report_aggregator_copy =
        report_aggregator_;
    flush_timer_ = options.periodic_timer->StartTimer(
        flush_interval, [check_aggregator_copy, report_aggregator_copy]() {
          Status status = check_aggregator_copy->Flush();
          if (!status.ok()) {
            GOOGLE_LOG(ERROR) << "Failed in Check::Flush() "
                              << status.error_message();
          }
          status = report_aggregator_copy->Flush();
          if (!status.ok()) {
            GOOGLE_LOG(ERROR) << "Failed in Report::Flush() "
                              << status.error_message();
          }
        });
  }
}

ServiceControlClientImpl::~ServiceControlClientImpl() {
  // Flush out all cached data
  FlushAll();
  if (flush_timer_) {
    flush_timer_->Stop();
  }

  // Disconnects all callback functions since this object is going away.
  // There could be some on_check_done() flying around. Each of them is
  // holding a ref_count to check_aggregator so check_aggregator is still
  // valid until all on_check_done() are called.
  // Each on_check_done() may call check_aggregator->CacheResponse() which
  // may call the flush callback. But since flush callback is disconnected,
  // we are OK.
  check_aggregator_->SetFlushCallback(NULL);
  report_aggregator_->SetFlushCallback(NULL);
}

void ServiceControlClientImpl::CheckFlushCallback(
    const CheckRequest& check_request) {
  CheckResponse* check_response = new CheckResponse;
  check_transport_->Check(check_request, check_response,
                          [check_response](Status status) {
                            delete check_response;
                            if (!status.ok()) {
                              GOOGLE_LOG(ERROR) << "Failed in Check call: "
                                                << status.error_message();
                            }
                          });
  ++send_checks_by_flush_;
}

void ServiceControlClientImpl::ReportFlushCallback(
    const ReportRequest& report_request) {
  ReportResponse* report_response = new ReportResponse;
  report_transport_->Report(report_request, report_response,
                     [report_response](Status status) {
                       delete report_response;
                       if (!status.ok()) {
                         GOOGLE_LOG(ERROR) << "Failed in Report call: "
                                           << status.error_message();
                       }
                     });
  ++send_reports_by_flush_;
  send_report_operations_ += report_request.operations_size();
}

void ServiceControlClientImpl::InternalCheck(CheckTransport* check_transport,
                                             const CheckRequest& check_request,
                                             CheckResponse* check_response,
                                             DoneCallback on_check_done) {
  ++total_called_checks_;
  if (check_transport == NULL) {
    on_check_done(Status(Code::INVALID_ARGUMENT, "transport is NULL."));
    return;
  }

  Status status = check_aggregator_->Check(check_request, check_response);
  if (status.error_code() == Code::NOT_FOUND) {
    // Makes a copy of check_request so that on_done() callback can use
    // it to call CacheResponse.
    CheckRequest* check_request_copy = new CheckRequest(check_request);
    std::shared_ptr<CheckAggregator> check_aggregator_copy = check_aggregator_;
    check_transport->Check(*check_request_copy, check_response,
                           [check_aggregator_copy, check_request_copy,
                            check_response, on_check_done](Status status) {
                             if (status.ok()) {
                               check_aggregator_copy->CacheResponse(
                                   *check_request_copy, *check_response);
                             } else {
                               GOOGLE_LOG(ERROR) << "Failed in Check call: "
                                                 << status.error_message();
                             }
                             delete check_request_copy;
                             on_check_done(status);
                           });
    ++send_checks_in_flight_;
    return;
  }
  on_check_done(status);
}

void ServiceControlClientImpl::Check(const CheckRequest& check_request,
                                     CheckResponse* check_response,
                                     DoneCallback on_check_done) {
  InternalCheck(check_transport_.get(), check_request, check_response,
                on_check_done);
}

Status ServiceControlClientImpl::Check(const CheckRequest& check_request,
                                       CheckResponse* check_response) {
  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  Check(check_request, check_response,
        [&status_promise](Status status) { status_promise.set_value(status); });

  status_future.wait();
  return status_future.get();
}

void ServiceControlClientImpl::Check(
    CheckTransport* check_transport,
    const ::google::api::servicecontrol::v1::CheckRequest& check_request,
    ::google::api::servicecontrol::v1::CheckResponse* check_response,
    DoneCallback on_check_done) {
  InternalCheck(check_transport, check_request, check_response, on_check_done);
}

void ServiceControlClientImpl::InternalReport(
    ReportTransport* report_transport, const ReportRequest& report_request,
    ReportResponse* report_response, DoneCallback on_report_done) {
  ++total_called_reports_;
  if (report_transport == NULL) {
    on_report_done(Status(Code::INVALID_ARGUMENT, "transport is NULL."));
    return;
  }

  Status status = report_aggregator_->Report(report_request);
  if (status.error_code() == Code::NOT_FOUND) {
    report_transport->Report(report_request, report_response, on_report_done);
    ++send_reports_in_flight_;
    send_report_operations_ += report_request.operations_size();
    return;
  }
  on_report_done(status);
}

void ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                      ReportResponse* report_response,
                                      DoneCallback on_report_done) {
  InternalReport(report_transport_.get(), report_request, report_response,
                 on_report_done);
}

Status ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                        ReportResponse* report_response) {
  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  Report(
      report_request, report_response,
      [&status_promise](Status status) { status_promise.set_value(status); });

  status_future.wait();
  return status_future.get();
}

void ServiceControlClientImpl::Report(
    ReportTransport* report_transport,
    const ::google::api::servicecontrol::v1::ReportRequest& report_request,
    ::google::api::servicecontrol::v1::ReportResponse* report_response,
    DoneCallback on_report_done) {
  InternalReport(report_transport, report_request, report_response,
                 on_report_done);
}

Status ServiceControlClientImpl::GetStatistics(Statistics* stat) const {
  stat->total_called_checks = total_called_checks_;
  stat->send_checks_by_flush = send_checks_by_flush_;
  stat->send_checks_in_flight = send_checks_in_flight_;
  stat->total_called_reports = total_called_reports_;
  stat->send_reports_by_flush = send_reports_by_flush_;
  stat->send_reports_in_flight = send_reports_in_flight_;
  stat->send_report_operations = send_report_operations_;
  return Status::OK;
}

int ServiceControlClientImpl::GetNextFlushInterval() {
  int check_interval = check_aggregator_->GetNextFlushInterval();
  int report_interval = report_aggregator_->GetNextFlushInterval();
  if (check_interval < 0) {
    return report_interval;
  } else if (report_interval < 0) {
    return check_interval;
  } else {
    return std::min(check_interval, report_interval);
  }
}

Status ServiceControlClientImpl::Flush() {
  Status check_status = check_aggregator_->Flush();
  Status report_status = report_aggregator_->Flush();
  if (!check_status.ok()) {
    return check_status;
  } else {
    return report_status;
  }
}

Status ServiceControlClientImpl::FlushAll() {
  Status check_status = check_aggregator_->FlushAll();
  Status report_status = report_aggregator_->FlushAll();
  if (!check_status.ok()) {
    return check_status;
  } else {
    return report_status;
  }
}

// Creates a ServiceControlClient object.
std::unique_ptr<ServiceControlClient> CreateServiceControlClient(
    const std::string& service_name, ServiceControlClientOptions& options) {
  return std::unique_ptr<ServiceControlClient>(
      new ServiceControlClientImpl(service_name, options));
}

}  // namespace service_control_client
}  // namespace google
