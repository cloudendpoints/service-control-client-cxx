/* Copyright 2021 Google Inc. All Rights Reserved.

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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_MOCKS_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_MOCKS_H_

#include "gmock/gmock.h"
#include "include/service_control_client_factory.h"
#include "include/service_control_client.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {
namespace testing {

class MockServiceControlClient : public ServiceControlClient {
 public:
  MOCK_METHOD(void, Check, (
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done));

  MOCK_METHOD(::google::protobuf::util::Status, Check, (
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response));

  MOCK_METHOD(void, Check, (
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done, TransportCheckFunc check_transport));

  MOCK_METHOD(void, Quota, (
      const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
          quota_request,
      ::google::api::servicecontrol::v1::AllocateQuotaResponse* quota_response,
      DoneCallback on_quota_done));

  MOCK_METHOD(::google::protobuf::util::Status, Quota, (
      const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
          quota_request,
      ::google::api::servicecontrol::v1::AllocateQuotaResponse*
          quota_response));

  MOCK_METHOD(void, Quota, (
      const ::google::api::servicecontrol::v1::AllocateQuotaRequest&
          quota_request,
      ::google::api::servicecontrol::v1::AllocateQuotaResponse* quota_response,
      DoneCallback on_quota_done, TransportQuotaFunc quota_transport));

  MOCK_METHOD(void, Report, (
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done));

  MOCK_METHOD(::google::protobuf::util::Status, Report, (
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response));

  MOCK_METHOD(void, Report, (
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done, TransportReportFunc report_transport));

  MOCK_METHOD(::google::protobuf::util::Status, GetStatistics,(
      Statistics* stat), (const));
};

class MockServiceControlClientFactory : public ServiceControlClientFactory {
 public:
  MOCK_METHOD(std::unique_ptr<ServiceControlClient>, CreateClient,
              (const std::string& service_name, const std::string& service_config_id,
    ServiceControlClientOptions& options), (const));
};

}  // namespace testing
}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_MOCKS_H_
