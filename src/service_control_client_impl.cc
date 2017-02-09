/* Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "src/quota_aggregator_impl.h"
#include "src/service_control_client_impl.h"

#include "google/protobuf/stubs/logging.h"
#include "utils/thread.h"

using std::string;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::AllocateQuotaRequest;
using ::google::api::servicecontrol::v1::AllocateQuotaResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {

ServiceControlClientImpl::ServiceControlClientImpl(
    const string& service_name, const std::string& service_config_id,
    ServiceControlClientOptions& options)
    : service_name_(service_name) {
  check_aggregator_ =
      CreateCheckAggregator(service_name, service_config_id,
                            options.check_options, options.metric_kinds);

  quota_aggregator_ = CreateAllocateQuotaAggregator(
      service_name, service_config_id, options.quota_options,
      options.metric_kinds);

  report_aggregator_ =
      CreateReportAggregator(service_name, service_config_id,
                             options.report_options, options.metric_kinds);

  quota_transport_ = options.quota_transport;
  check_transport_ = options.check_transport;
  report_transport_ = options.report_transport;

  total_called_checks_ = 0;
  send_checks_by_flush_ = 0;
  send_checks_in_flight_ = 0;

  total_called_quotas_ = 0;
  send_quotas_by_flush_ = 0;
  send_quotas_in_flight_ = 0;

  total_called_reports_ = 0;
  send_reports_by_flush_ = 0;
  send_reports_in_flight_ = 0;
  send_report_operations_ = 0;

  check_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::CheckFlushCallback, this,
                std::placeholders::_1));

  quota_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::AllocateQuotaFlushCallback, this,
                std::placeholders::_1));

  report_aggregator_->SetFlushCallback(
      std::bind(&ServiceControlClientImpl::ReportFlushCallback, this,
                std::placeholders::_1));

  int flush_interval = GetNextFlushInterval();
  if (options.periodic_timer && flush_interval > 0) {
    // Class members cannot be captured in lambda. We need to make a copy to
    // support C++11.
    std::shared_ptr<CheckAggregator> check_aggregator_copy = check_aggregator_;
    std::shared_ptr<QuotaAggregator> quota_aggregator_copy = quota_aggregator_;
    std::shared_ptr<ReportAggregator> report_aggregator_copy =
        report_aggregator_;

    flush_timer_ = options.periodic_timer(
        flush_interval, [check_aggregator_copy, quota_aggregator_copy,
                         report_aggregator_copy]() {

          Status status = check_aggregator_copy->Flush();
          if (!status.ok()) {
            GOOGLE_LOG(ERROR)
                << "Failed in Check::Flush() " << status.error_message();
          }

          status = quota_aggregator_copy->Flush();
          if (!status.ok()) {
            GOOGLE_LOG(ERROR) << "Failed in AllocateQuota::Flush() "
                              << status.error_message();
          }

          status = report_aggregator_copy->Flush();
          if (!status.ok()) {
            GOOGLE_LOG(ERROR)
                << "Failed in Report::Flush() " << status.error_message();
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
  quota_aggregator_->SetFlushCallback(NULL);
  report_aggregator_->SetFlushCallback(NULL);
}

void ServiceControlClientImpl::AllocateQuotaFlushCallback(
    const AllocateQuotaRequest& quota_request) {
  AllocateQuotaResponse* quota_response = new AllocateQuotaResponse;

  quota_transport_(
      quota_request, quota_response,
      [this, quota_request, quota_response](Status status) {
        GOOGLE_LOG(INFO) << "Refreshed the quota cache for "
                         << quota_response->operation_id();

        this->quota_aggregator_->CacheResponse(quota_request, *quota_response);

        delete quota_response;

        if (!status.ok()) {
          GOOGLE_LOG(ERROR)
              << "Failed in AllocateQuota call: " << status.error_message();
        }
      });

  ++send_quotas_by_flush_;
}

void ServiceControlClientImpl::CheckFlushCallback(
    const CheckRequest& check_request) {
  CheckResponse* check_response = new CheckResponse;
  check_transport_(
      check_request, check_response, [check_response](Status status) {
        delete check_response;
        if (!status.ok()) {
          GOOGLE_LOG(ERROR)
              << "Failed in Check call: " << status.error_message();
        }
      });
  ++send_checks_by_flush_;
}

void ServiceControlClientImpl::ReportFlushCallback(
    const ReportRequest& report_request) {
  ReportResponse* report_response = new ReportResponse;
  report_transport_(
      report_request, report_response, [report_response](Status status) {
        delete report_response;
        if (!status.ok()) {
          GOOGLE_LOG(ERROR)
              << "Failed in Report call: " << status.error_message();
        }
      });
  ++send_reports_by_flush_;
  send_report_operations_ += report_request.operations_size();
}

void ServiceControlClientImpl::Check(const CheckRequest& check_request,
                                     CheckResponse* check_response,
                                     DoneCallback on_check_done,
                                     TransportCheckFunc check_transport) {
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
    check_transport(*check_request_copy, check_response,
                    [check_aggregator_copy, check_request_copy, check_response,
                     on_check_done](Status status) {
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
  Check(check_request, check_response, on_check_done, check_transport_);
}

Status ServiceControlClientImpl::Check(const CheckRequest& check_request,
                                       CheckResponse* check_response) {
  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  Check(check_request, check_response, [&status_promise](Status status) {
    // Need to move the promise as it must be owned by the thread where this
    // lambda is executed rather than the thread where the original Check()
    // call is executed.
    // Otherwise, if we call std::promise::set_value(), the original thread will
    // be unblocked and it might destroy the promise object before set_value()
    // has a chance to finish.
    StatusPromise moved_promise(std::move(status_promise));
    moved_promise.set_value(status);
  });

  status_future.wait();
  return status_future.get();
}

Status ServiceControlClientImpl::convertResponseStatus(
    const ::google::api::servicecontrol::v1::AllocateQuotaResponse& response) {
  if (response.allocate_errors().size() == 0) {
    return Status::OK;
  }

  const ::google::api::servicecontrol::v1::QuotaError& error =
      response.allocate_errors().Get(0);

  switch (error.code()) {
    case ::google::api::servicecontrol::v1::QuotaError::UNSPECIFIED:
      // This is never used.
      break;

    case ::google::api::servicecontrol::v1::QuotaError::RESOURCE_EXHAUSTED:
      // Quota allocation failed.
      // Same as [google.rpc.Code.RESOURCE_EXHAUSTED][].
      return Status(Code::PERMISSION_DENIED, "Quota allocation failed.");

    case ::google::api::servicecontrol::v1::QuotaError::PROJECT_SUSPENDED:
      // Consumer project has been suspended.
      return Status(Code::PERMISSION_DENIED, "Project suspended.");

    case ::google::api::servicecontrol::v1::QuotaError::SERVICE_NOT_ENABLED:
      // Consumer has not enabled the service.
      return Status(Code::PERMISSION_DENIED,
                    std::string("API ") + service_name_ +
                        " is not enabled for the project.");

    case ::google::api::servicecontrol::v1::QuotaError::BILLING_NOT_ACTIVE:
      // Consumer cannot access the service because billing is disabled.
      return Status(Code::PERMISSION_DENIED,
                    std::string("API ") + service_name_ +
                        " has billing disabled. Please enable it.");

    case ::google::api::servicecontrol::v1::QuotaError::PROJECT_DELETED:
    // Consumer's project has been marked as deleted (soft deletion).
    case ::google::api::servicecontrol::v1::QuotaError::PROJECT_INVALID:
      // Consumer's project number or ID does not represent a valid project.
      return Status(Code::INVALID_ARGUMENT,
                    "Client project not valid. Please pass a valid project.");

    case ::google::api::servicecontrol::v1::QuotaError::IP_ADDRESS_BLOCKED:
      // IP address of the consumer is invalid for the specific consumer
      // project.
      return Status(Code::PERMISSION_DENIED, "IP address blocked.");

    case ::google::api::servicecontrol::v1::QuotaError::REFERER_BLOCKED:
      // Referer address of the consumer request is invalid for the specific
      // consumer project.
      return Status(Code::PERMISSION_DENIED, "Referer blocked.");

    case ::google::api::servicecontrol::v1::QuotaError::CLIENT_APP_BLOCKED:
      // Client application of the consumer request is invalid for the
      // specific consumer project.
      return Status(Code::PERMISSION_DENIED, "Client app blocked.");

    case ::google::api::servicecontrol::v1::QuotaError::API_KEY_INVALID:
      // Specified API key is invalid.
      return Status(Code::INVALID_ARGUMENT,
                    "API key not valid. Please pass a valid API key.");

    case ::google::api::servicecontrol::v1::QuotaError::API_KEY_EXPIRED:
      // Specified API Key has expired.
      return Status(Code::INVALID_ARGUMENT,
                    "API key expired. Please renew the API key.");

    case ::google::api::servicecontrol::v1::QuotaError::
        PROJECT_STATUS_UNVAILABLE:
    // The backend server for looking up project id/number is unavailable.
    case ::google::api::servicecontrol::v1::QuotaError::
        SERVICE_STATUS_UNAVAILABLE:
    // The backend server for checking service status is unavailable.
    case ::google::api::servicecontrol::v1::QuotaError::
        BILLING_STATUS_UNAVAILABLE:
      // The backend server for checking billing status is unavailable.
      // Fail open for internal server errors per recommendation
      return Status::OK;

    default:
      return Status(
          Code::INTERNAL,
          std::string("Request blocked due to unsupported error code: ") +
              std::to_string(error.code()));
  }

  return Status::OK;
}

void ServiceControlClientImpl::Quota(
    const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
        quota_request,
    ::google::api::servicecontrol::v1::AllocateQuotaResponse* quota_response,
    DoneCallback on_quota_done, TransportQuotaFunc check_transport) {
  ++total_called_quotas_;
  if (check_transport == NULL) {
    on_quota_done(Status(Code::INVALID_ARGUMENT, "transport is NULL."));
    return;
  }

  Status status = quota_aggregator_->Quota(quota_request, quota_response);
  if (status.error_code() == Code::NOT_FOUND) {
    // Makes a copy of check_request so that on_done() callback can use
    // it to call CacheResponse.
    ::google::api::servicecontrol::v1::AllocateQuotaRequest*
        quota_request_copy =
            new ::google::api::servicecontrol::v1::AllocateQuotaRequest(
                quota_request);

    std::shared_ptr<QuotaAggregator> quota_aggregator_copy = quota_aggregator_;
    check_transport(*quota_request_copy, quota_response,
                    [this, quota_aggregator_copy, quota_request_copy,
                     quota_response, on_quota_done](Status status) {

                      if (status.ok()) {
                        quota_aggregator_copy->CacheResponse(
                            *quota_request_copy, *quota_response);
                      } else {
                        GOOGLE_LOG(ERROR) << "Failed in Check call: "
                                          << status.error_message();
                      }

                      delete quota_request_copy;

                      on_quota_done(convertResponseStatus(*quota_response));
                    });

    ++send_quotas_in_flight_;
    return;

  } else if (status.error_code() == Code::INVALID_ARGUMENT ||
             status.error_code() == Code::UNAVAILABLE) {
    on_quota_done(status);
  } else {
    // Status::OK, return response status from AllocateQuotaResponse
    on_quota_done(convertResponseStatus(*quota_response));
  }
}

// An async quota call.
void ServiceControlClientImpl::Quota(
    const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
        quota_request,
    ::google::api::servicecontrol::v1::AllocateQuotaResponse* quota_response,
    DoneCallback on_quota_done) {
  Quota(quota_request, quota_response, on_quota_done, quota_transport_);
}

// A sync quota call.
::google::protobuf::util::Status ServiceControlClientImpl::Quota(
    const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
        quota_request,
    ::google::api::servicecontrol::v1::AllocateQuotaResponse* quota_response) {
  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  Quota(quota_request, quota_response, [&status_promise](Status status) {
    // Need to move the promise as it must be owned by the thread where this
    // lambda is executed rather than the thread where the original Check()
    // call is executed.
    // Otherwise, if we call std::promise::set_value(), the original thread will
    // be unblocked and it might destroy the promise object before set_value()
    // has a chance to finish.
    StatusPromise moved_promise(std::move(status_promise));
    moved_promise.set_value(status);
  });

  status_future.wait();
  return status_future.get();
}

void ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                      ReportResponse* report_response,
                                      DoneCallback on_report_done,
                                      TransportReportFunc report_transport) {
  ++total_called_reports_;
  if (report_transport == NULL) {
    on_report_done(Status(Code::INVALID_ARGUMENT, "transport is NULL."));
    return;
  }

  Status status = report_aggregator_->Report(report_request);
  if (status.error_code() == Code::NOT_FOUND) {
    report_transport(report_request, report_response, on_report_done);
    ++send_reports_in_flight_;
    send_report_operations_ += report_request.operations_size();
    return;
  }
  on_report_done(status);
}

void ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                      ReportResponse* report_response,
                                      DoneCallback on_report_done) {
  Report(report_request, report_response, on_report_done, report_transport_);
}

Status ServiceControlClientImpl::Report(const ReportRequest& report_request,
                                        ReportResponse* report_response) {
  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  Report(report_request, report_response, [&status_promise](Status status) {
    // Need to move the promise as it must be owned by the thread where this
    // lambda is executed rather than the thread where the original Report()
    // call is executed.
    // Otherwise, if we call std::promise::set_value(), the original thread will
    // be unblocked and it might destroy the promise object before set_value()
    // has a chance to finish.
    StatusPromise moved_promise(std::move(status_promise));
    moved_promise.set_value(status);
  });

  status_future.wait();
  return status_future.get();
}

Status ServiceControlClientImpl::GetStatistics(Statistics* stat) const {
  stat->total_called_checks = total_called_checks_;
  stat->send_checks_by_flush = send_checks_by_flush_;
  stat->send_checks_in_flight = send_checks_in_flight_;

  stat->total_called_quotas = total_called_quotas_;
  stat->send_quotas_by_flush = send_quotas_by_flush_;
  stat->send_quotas_in_flight = send_quotas_in_flight_;

  stat->total_called_reports = total_called_reports_;
  stat->send_reports_by_flush = send_reports_by_flush_;
  stat->send_reports_in_flight = send_reports_in_flight_;
  stat->send_report_operations = send_report_operations_;
  return Status::OK;
}

// TODO(jaebong) Consider quotas interbal
int ServiceControlClientImpl::GetNextFlushInterval() {
  int check_interval = check_aggregator_->GetNextFlushInterval();
  // int quota_interval = check_aggregator_->GetNextFlushInterval();
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
    const std::string& service_name, const std::string& service_config_id,
    ServiceControlClientOptions& options) {
  return std::unique_ptr<ServiceControlClient>(
      new ServiceControlClientImpl(service_name, service_config_id, options));
}

}  // namespace service_control_client
}  // namespace google
