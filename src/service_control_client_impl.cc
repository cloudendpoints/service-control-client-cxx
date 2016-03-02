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

ServiceControlControlImpl::ServiceControlControlImpl(
    const string& service_name, const CheckAggregationOptions& check_options,
    const ReportAggregationOptions& report_options,
    std::shared_ptr<MetricKindMap> metric_kinds) {
  check_aggregator_ = std::move(
      CreateCheckAggregator(service_name, check_options, metric_kinds));
  report_aggregator_ = std::move(
      CreateReportAggregator(service_name, report_options, metric_kinds));

  check_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlControlImpl::CheckFlushCallback, this,
                std::placeholders::_1));
  report_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlControlImpl::ReportFlushCallback, this,
                std::placeholders::_1));
}

ServiceControlControlImpl::~ServiceControlControlImpl() {
  check_aggregator_->SetFlushCallback(NULL);
  report_aggregator_->SetFlushCallback(NULL);
}

void ServiceControlControlImpl::CheckFlushCallback(
    const CheckRequest& check_request) {
  ReportRequest* check_request_copy = new ReportRequest(check_request);
  CheckResponse* check_response = new CheckResponse;
  non_blocking_transport_->Check(
      *check_request_copy, check_response,
      [check_aggregator_, check_request_copy, check_response](Status status) {
        if (status.ok()) {
          check_aggregator_->CacheResponse(*check_request_copy,
                                           *check_response);
        } else {
          GOOGLE_LOG(ERROR) << "Failed in Check call: "
                            << status.error_message();
        }
        delete check_request_copy;
        delete check_response;
      });
}

void ServiceControlControlImpl::ReportFlushCallback(
    const ReportRequest& report_request) {
  ReportResponse* report_response = new ReportResponse;
  non_blocking_transport_->Report(
      report_request, report_response,
      [](Status status) { delete report_response; });
}

void ServiceControlControlImpl::Check(
    const ::google::api::servicecontrol::v1::CheckRequest& check_request,
    ::google::api::servicecontrol::v1::CheckResponse* check_response,
    DoneCallback on_check_done) {
  if (non_blocking_transport_ == NULL) {
    on_report_done(
        Status(Code::INVALID_ARGUMENT, "Non-blocking transport is NULL."));
    return;
  }

  Status status = check_aggregator_->Check(check_request, check_response);
  if (status.Code() == Code::NOT_FOUND) {
    ReportRequest* check_request_copy = new ReportRequest(check_request);
    non_blocking_transport_->Check(
        *check_request_copy, check_response,
        [check_aggregator_, check_request_copy, check_response,
         on_check_done](Status status) {
          if (status.ok()) {
            check_aggregator_->CacheResponse(*check_request_copy,
                                             *check_response);
          }
          delete check_request_copy;
          on_check_done(status);
        });
    return;
  }
  on_report_done(status);
}

::google::protobuf::util::Status ServiceControlControlImpl::Blocking_Check(
    const ::google::api::servicecontrol::v1::CheckRequest& check_request,
    ::google::api::servicecontrol::v1::CheckResponse* check_response) {
  return Status(Code::UNIMPLEMENTED, "This method is not implemented yet.");
}

void ServiceControlControlImpl::Report(
    const ::google::api::servicecontrol::v1::ReportRequest& report_request,
    ::google::api::servicecontrol::v1::ReportResponse* report_response,
    DoneCallback on_report_done) {
  if (non_blocking_transport_ == NULL) {
    on_report_done(
        Status(Code::INVALID_ARGUMENT, "Non-blocking transport is NULL."));
    return;
  }

  Status status = report_aggregator_->Report(report_request);
  if (status.Code() == Code::NOT_FOUND) {
    non_blocking_transport_->Report(report_request, report_response,
                                    on_report_done);
    return;
  }
  on_report_done(status);
}

::google::protobuf::util::Status ServiceControlControlImpl::Blocking_Report(
    const ::google::api::servicecontrol::v1::ReportRequest& report_request,
    ::google::api::servicecontrol::v1::ReportResponse* report_response) {
  return Status(Code::UNIMPLEMENTED, "This method is not implemented yet.");
}

}  // namespace service_control_client
}  // namespace google
