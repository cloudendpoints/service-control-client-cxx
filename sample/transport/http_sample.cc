/* Copyright 2016 Google Inc. All Rights Reserved.

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
#include <fstream>
#include <future>
#include <iostream>
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/logging.h"
#include "google/protobuf/stubs/logging.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/text_format.h"
#include "include/service_control_client.h"
#include "sample/transport/http_transport.h"

using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::service_control_client::CheckAggregationOptions;
using ::google::service_control_client::ReportAggregationOptions;
using ::google::service_control_client::ServiceControlClient;
using ::google::service_control_client::ServiceControlClientOptions;
using ::google::service_control_client::TransportDoneFunc;
using ::google::service_control_client::sample::transport::LibCurlTransport;
using ::google::protobuf::util::Status;
using ::google::protobuf::TextFormat;

const char kCheckRequest[] = R"(
service_name: "echo-dot-esp-load-test.appspot.com"
operation {
  operation_id: "abced"
  operation_name: "EchoGetMessageAuthed"
  consumer_id: "project:esp-load-test"
  start_time: {
    seconds: 1000
    nanos: 2000
  }
  end_time: {
    seconds: 3000
    nanos: 4000
  }
}
)";

const char kReportRequest[] = R"(
service_name: "echo-dot-esp-load-test.appspot.com"
operations: {
  operation_id: "operation-1"
  operation_name: "EchoGetMessageAuthed"
  consumer_id: "project:esp-load-test"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  end_time {
    seconds: 3000
    nanos: 4000
  }
}
)";

void print_usage() {
  fprintf(stderr,
          "Missing Argument.\n"
          "Usage: http_sample server_url service_name auth_token.\n"
          "auth_token can be obtained by one of the followings: \n"
          "1) Fetching from GCP metadata server if this code is running  "
          "inside a Google Cloud Platform VM. \n"
          "2) Generating every one hour with service account secret file.\n"
          "    a) Create service account and download client-secret-file: \n"
          "       Google Cloud Console -> IAM & ADMIN -> Service Accounts \n"
          "        -> Create Service Account (with editor role)\n"
          "    b) Generate auth token: \n"
          "       gen-auth-token.sh\n"
          "       -s client-secret-file\n"
          "       -a "
          "https://servicecontrol.googleapis.com/"
          "google.api.servicecontrol.v1.ServiceController\n");
}

int main(int argc, char** argv) {
  if (argc <= 3) {
    print_usage();
    return 1;
  }

  std::string server_url = argv[1];
  std::string service_name = argv[2];
  std::string auth_token = argv[3];

  CheckRequest check_request;
  ReportRequest report_request;
  CheckResponse check_response;
  ReportResponse report_response;
  ::google::protobuf::TextFormat::ParseFromString(kCheckRequest,
                                                  &check_request);
  ::google::protobuf::TextFormat::ParseFromString(kReportRequest,
                                                  &report_request);

  // Initialize the sample transport.
  LibCurlTransport* transport =
      new LibCurlTransport(server_url, service_name, auth_token);

  // Initialize service control client.
  std::unique_ptr<ServiceControlClient> client;
  // Both check cache and report cache has been disabled. So each check/report
  // call at client will use the transport to call remote.
  ServiceControlClientOptions options(
      CheckAggregationOptions(-1 /*entries*/, 1000 /* refresh_interval_ms */,
                              1000 /*flush_interval_ms*/),
      ReportAggregationOptions(-1 /*entries*/, 1000 /* refresh_interval_ms */));

  options.check_transport = [transport](const CheckRequest& check_request,
                                        CheckResponse* check_response,
                                        TransportDoneFunc on_done) {
    transport->Check(check_request, check_response, on_done);
  };
  options.report_transport = [transport](const ReportRequest& report_request,
                                         ReportResponse* report_response,
                                         TransportDoneFunc on_done) {
    transport->Report(report_request, report_response, on_done);
  };

  client = CreateServiceControlClient(service_name, "", options);

  // Call Check.
  std::promise<Status> check_promise_status;
  std::future<Status> check_future_status = check_promise_status.get_future();
  client->Check(check_request, &check_response, [&check_promise_status](
                                                    const Status& status) {
    std::cout << "status is :" << status.ToString() << std::endl;
    std::promise<Status> moved_promise(std::move(check_promise_status));
    moved_promise.set_value(status);
  });

  // Call Report.
  std::promise<Status> report_promise_status;
  std::future<Status> report_future_status = report_promise_status.get_future();
  client->Report(report_request, &report_response, [&report_promise_status](
                                                       const Status& status) {
    std::cout << "status is :" << status.ToString() << std::endl;
    std::promise<Status> moved_promise(std::move(report_promise_status));
    moved_promise.set_value(status);
  });

  check_future_status.wait();
  report_future_status.wait();

  std::cout << "check response is:" << check_response.DebugString()
            << std::endl;
  std::cout << "report response is:" << report_response.DebugString()
            << std::endl;

  delete transport;
  return 0;
}
