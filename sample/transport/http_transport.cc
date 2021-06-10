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
#include "sample/transport/http_transport.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;

using ::google::protobuf::util::OkStatus;
using ::google::protobuf::util::StatusCode;
using ::google::protobuf::util::Status;

namespace google {
namespace service_control_client {
namespace sample {
namespace transport {
namespace {

Status ConvertHttpCodeToStatus(const long &http_code) {
  Status status;
  std::cout << "http_code is: " << http_code << std::endl;
  switch (http_code) {
    case 400:
      status = Status(StatusCode::kInvalidArgument, std::string("Bad Request."));
    case 403:
      status =
          Status(StatusCode::kPermissionDenied, std::string("Permission Denied."));
    case 404:
      status = Status(StatusCode::kNotFound, std::string("Not Found."));
    case 409:
      status = Status(StatusCode::kAborted, std::string("Conflict."));
    case 416:
      status = Status(StatusCode::kOutOfRange,
                      std::string("Requested Range Not Satisfiable."));
    case 429:
      status =
          Status(StatusCode::kResourceExhausted, std::string("Too Many Requests."));
    case 499:
      status = Status(StatusCode::kCancelled, std::string("Client Closed Request."));
    case 504:
      status = Status(StatusCode::kDeadlineExceeded, std::string("Gateway Timeout."));
    case 501:
      status = Status(StatusCode::kUnimplemented, std::string("Not Implemented."));
    case 503:
      status = Status(StatusCode::kUnavailable, std::string("Service Unavailable."));
    case 401:
      status = Status(StatusCode::kUnauthenticated, std::string("Unauthorized."));
    default: {
      if (http_code >= 200 && http_code < 300) {
        status = Status(StatusCode::kOk, std::string("OK."));

      } else if (http_code >= 400 && http_code < 500) {
        status =
            Status(StatusCode::kFailedPrecondition, std::string("Client Error."));
      } else if (http_code >= 500 && http_code < 600) {
        status = Status(StatusCode::kInternal, std::string("Server Error."));
      } else
        status = Status(StatusCode::kUnknown, std::string("Unknown Error."));
    }
  }
  return status;
}

static size_t ResultBodyCallback(void *data, size_t size, size_t nmemb,
                                 void *instance) {
  size_t data_len = size * nmemb;

  if (data_len > 0) {
    ((std::string *)instance)->append((char *)data, size * nmemb);
  }
  return data_len;
}

Status SendHttp(const std::string &url, const std::string &auth_header,
                const std::string &request_body, std::string *response_body) {
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url.data());

  struct curl_slist *list = NULL;
  list = curl_slist_append(list, "Content-Type: application/x-protobuf");
  list = curl_slist_append(list, "X-GFE-SSL: yes");

  list = curl_slist_append(list, auth_header.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

  // 0L will not include headers in response body. 1L will include headers in
  // response body.
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
  curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST,
                   "ALL:!aNULL:!LOW:!EXPORT:!SSLv2");

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.data());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ResultBodyCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_body);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

  CURLcode res = curl_easy_perform(curl);
  Status status = OkStatus();
  if (res != CURLE_OK) {
    std::cout << "curl easy_perform() failed." << std::endl;
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    Status status = ConvertHttpCodeToStatus(http_code);
  }

  curl_easy_cleanup(curl);
  return status;
}

}  // namespace

void LibCurlTransport::Check(
    const ::google::api::servicecontrol::v1::CheckRequest &request,
    ::google::api::servicecontrol::v1::CheckResponse *response,
    TransportDoneFunc on_done) {
  std::string *request_body = new std::string();
  request.SerializeToString(request_body);

  std::thread t([request_body, response, on_done, this]() {
    std::string response_body;
    Status status = SendHttp(this->check_url_, this->auth_token_header_,
                             *request_body, &response_body);
    delete request_body;
    if (status.ok()) {
      if (!response->ParseFromString(response_body)) {
        status = Status(StatusCode::kInvalidArgument,
                        std::string("Cannot parse response to proto."));
      } else {
        std::cout << "response parsed from string: " << response->DebugString()
                  << std::endl;
      }
    }
    on_done(status);

  });
  t.detach();
}

void LibCurlTransport::Report(
    const ::google::api::servicecontrol::v1::ReportRequest &request,
    ::google::api::servicecontrol::v1::ReportResponse *response,
    TransportDoneFunc on_done) {
  std::string *request_body = new std::string();
  request.SerializeToString(request_body);

  std::thread t([request_body, response, on_done, this]() {
    std::string response_body;
    Status status = SendHttp(this->report_url_, this->auth_token_header_,
                             *request_body, &response_body);
    delete request_body;
    if (status.ok()) {
      if (!response->ParseFromString(response_body)) {
        status = Status(StatusCode::kInvalidArgument,
                        std::string("Cannot parse response to proto."));
      } else {
        std::cout << "response parsed from string: " << response->DebugString()
                  << std::endl;
      }
    }
    on_done(status);
  });
  t.detach();
}

}  // namespace transport
}  // namespace sample
}  // namespace service_control_client
}  // namespace google
