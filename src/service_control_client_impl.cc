#include "src/service_control_client_impl.h"

#include "google/protobuf/stubs/logging.h"

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

  transport_ = std::move(options.transport);

  check_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::CheckFlushCallback, this,
                std::placeholders::_1));
  report_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::ReportFlushCallback, this,
                std::placeholders::_1));
}

ServiceControlClientImpl::~ServiceControlClientImpl() {
  // Flush out all cached data
  FlushAll();

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
  transport_->Check(check_request, check_response,
                    [check_response](Status status) {
                      delete check_response;
                      if (!status.ok()) {
                        GOOGLE_LOG(ERROR) << "Failed in Check call: "
                                          << status.error_message();
                      }
                    });
}

void ServiceControlClientImpl::ReportFlushCallback(
    const ReportRequest& report_request) {
  ReportResponse* report_response = new ReportResponse;
  transport_->Report(report_request, report_response,
                     [report_response](Status status) {
                       delete report_response;
                       if (!status.ok()) {
                         GOOGLE_LOG(ERROR) << "Failed in Report call: "
                                           << status.error_message();
                       }
                     });
}

void ServiceControlClientImpl::Check(const CheckRequest& check_request,
                                     CheckResponse* check_response,
                                     DoneCallback on_check_done) {
  if (transport_ == NULL) {
    on_check_done(
        Status(Code::INVALID_ARGUMENT, "Non-blocking transport is NULL."));
    return;
  }

  Status status = check_aggregator_->Check(check_request, check_response);
  if (status.error_code() == Code::NOT_FOUND) {
    // Makes a copy of check_request so that on_done() callback can use
    // it to call CacheResponse.
    CheckRequest* check_request_copy = new CheckRequest(check_request);
    transport_->Check(*check_request_copy, check_response, [
      check_aggregator_copy = check_aggregator_, check_request_copy,
      check_response, on_check_done
    ](Status status) {
      if (status.ok()) {
        check_aggregator_copy->CacheResponse(*check_request_copy,
                                             *check_response);
      } else {
        GOOGLE_LOG(ERROR) << "Failed in Check call: " << status.error_message();
      }
      delete check_request_copy;
      on_check_done(status);
    });
    return;
  }
  on_check_done(status);
}

Status ServiceControlClientImpl::BlockingCheck(
    const CheckRequest& check_request, CheckResponse* check_response) {
  return Status(Code::UNIMPLEMENTED, "This method is not implemented yet.");
}

void ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                      ReportResponse* report_response,
                                      DoneCallback on_report_done) {
  if (transport_ == NULL) {
    on_report_done(
        Status(Code::INVALID_ARGUMENT, "Non-blocking transport is NULL."));
    return;
  }

  Status status = report_aggregator_->Report(report_request);
  if (status.error_code() == Code::NOT_FOUND) {
    transport_->Report(report_request, report_response, on_report_done);
    return;
  }
  on_report_done(status);
}

Status ServiceControlClientImpl::BlockingReport(
    const ReportRequest& report_request, ReportResponse* report_response) {
  return Status(Code::UNIMPLEMENTED, "This method is not implemented yet.");
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
