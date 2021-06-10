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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_MOCK_TRANSPORT_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_MOCK_TRANSPORT_H_

#include "include/service_control_client.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"
#include "utils/thread.h"

#include <vector>

using std::string;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::AllocateQuotaRequest;
using ::google::api::servicecontrol::v1::AllocateQuotaResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::CancelledError;
using ::google::protobuf::util::OkStatus;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::StatusCode;
using ::google::protobuf::util::UnknownError;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::_;

namespace google {
namespace service_control_client {
namespace {

// A mocking class to mock CheckTransport interface.
class MockCheckTransport {
 public:
  MOCK_METHOD3(Check,
               void(const CheckRequest&, CheckResponse*, TransportDoneFunc));
  TransportCheckFunc GetFunc() {
    return [this](const CheckRequest& request, CheckResponse* response,
                  TransportDoneFunc on_done) {
      this->Check(request, response, on_done);
    };
  }

  MockCheckTransport() : check_response_(NULL) {
    // To avoid vector resize which will cause segmentation fault.
    on_done_vector_.reserve(100);
  }

  ~MockCheckTransport() {
    for (auto& callback_thread : callback_threads_) {
      callback_thread->join();
    }
  }

  // The done callback is stored in on_done_. It MUST be called later.
  void CheckWithStoredCallback(const CheckRequest& request,
                               CheckResponse* response,
                               TransportDoneFunc on_done) {
    check_request_ = request;
    if (check_response_) {
      *response = *check_response_;
    }
    on_done_vector_.push_back(on_done);
  }

  // The done callback is called right away (in place).
  void CheckWithInplaceCallback(const CheckRequest& request,
                                CheckResponse* response,
                                TransportDoneFunc on_done) {
    check_request_ = request;
    if (check_response_) {
      *response = *check_response_;
    }
    on_done(done_status_);
  }

  // The done callback is called from a separate thread with check_status_
  void CheckUsingThread(const CheckRequest& request, CheckResponse* response,
                        TransportDoneFunc on_done) {
    check_request_ = request;
    Status done_status = done_status_;
    CheckResponse* check_response = check_response_;
    callback_threads_.push_back(std::unique_ptr<Thread>(
        new Thread([on_done, done_status, check_response, response]() {
          if (check_response) {
            *response = *check_response;
          }
          on_done(done_status);
        })));
  }

  // Saved check_request from mocked Transport::Check() call.
  CheckRequest check_request_;
  // If not NULL, the check response to send for mocked Transport::Check() call.
  CheckResponse* check_response_;

  // saved on_done callback from either Transport::Check() or
  // Transport::Report().
  std::vector<TransportDoneFunc> on_done_vector_;
  // The status to send in on_done call back for Check() or Report().
  Status done_status_;
  // A vector to store thread objects used to call on_done callback.
  std::vector<std::unique_ptr<std::thread>> callback_threads_;
};

// A mocking class to mock QuotaTransport interface.
class MockQuotaTransport {
 public:
  MOCK_METHOD3(Quota, void(const AllocateQuotaRequest&, AllocateQuotaResponse*,
                           TransportDoneFunc));

  TransportQuotaFunc GetFunc() {
    return [this](const AllocateQuotaRequest& request,
                  AllocateQuotaResponse* response, TransportDoneFunc on_done) {
      this->Quota(request, response, on_done);
    };
  }

  MockQuotaTransport() : quota_response_(NULL) {
    // To avoid vector resize which will cause segmentation fault.
    on_done_vector_.reserve(100);
  }

  ~MockQuotaTransport() {
    for (auto& callback_thread : callback_threads_) {
      callback_thread->join();
    }
  }

  // The done callback is stored in on_done_. It MUST be called later.
  void AllocateQuotaWithStoredCallback(const AllocateQuotaRequest& request,
                                       AllocateQuotaResponse* response,
                                       TransportDoneFunc on_done) {
    quota_request_ = request;
    if (quota_response_) {
      *response = *quota_response_;
    }
    on_done_vector_.push_back(on_done);
  }

  // The done callback is called right away (in place).
  void AllocateQuotaWithInplaceCallback(const AllocateQuotaRequest& request,
                                        AllocateQuotaResponse* response,
                                        TransportDoneFunc on_done) {
    quota_request_ = request;
    if (quota_response_) {
      *response = *quota_response_;
    }
    on_done(done_status_);
  }

  // The done callback is called from a separate thread with quota_status_
  void AllocateQuotaUsingThread(const AllocateQuotaRequest& request,
                                AllocateQuotaResponse* response,
                                TransportDoneFunc on_done) {
    quota_request_ = request;
    Status done_status = done_status_;
    AllocateQuotaResponse* quota_response = quota_response_;
    callback_threads_.push_back(std::unique_ptr<Thread>(
        new Thread([on_done, done_status, quota_response, response]() {
          if (quota_response) {
            *response = *quota_response;
          }
          on_done(done_status);
        })));
  }

  // Saved quota_request from mocked Transport::Quota() call.
  AllocateQuotaRequest quota_request_;
  // If not NULL, the quota response to send for mocked Transport::Quota() call.
  AllocateQuotaResponse* quota_response_;

  // saved on_done callback from either Transport::Quota() or
  // Transport::Report().
  std::vector<TransportDoneFunc> on_done_vector_;
  // The status to send in on_done call back for Quota() or Report().
  Status done_status_;
  // A vector to store thread objects used to call on_done callback.
  std::vector<std::unique_ptr<std::thread>> callback_threads_;
};

// A mocking class to mock ReportTransport interface.
class MockReportTransport {
 public:
  MOCK_METHOD3(Report,
               void(const ReportRequest&, ReportResponse*, TransportDoneFunc));
  TransportReportFunc GetFunc() {
    return [this](const ReportRequest& request, ReportResponse* response,
                  TransportDoneFunc on_done) {
      this->Report(request, response, on_done);
    };
  }

  MockReportTransport() : report_response_(NULL) {
    // To avoid vector resize which will cause segmentation fault.
    on_done_vector_.reserve(100);
  }

  ~MockReportTransport() {
    for (auto& callback_thread : callback_threads_) {
      callback_thread->join();
    }
  }

  // The done callback is stored in on_done_. It MUST be called later.
  void ReportWithStoredCallback(const ReportRequest& request,
                                ReportResponse* response,
                                TransportDoneFunc on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    on_done_vector_.push_back(on_done);
  }

  // The done callback is called right away (in place).
  void ReportWithInplaceCallback(const ReportRequest& request,
                                 ReportResponse* response,
                                 TransportDoneFunc on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    on_done(done_status_);
  }

  // The done callback is called from a separate thread with done_status_
  void ReportUsingThread(const ReportRequest& request, ReportResponse* response,
                         TransportDoneFunc on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    Status done_status = done_status_;
    callback_threads_.push_back(std::unique_ptr<Thread>(
        new Thread([on_done, done_status]() { on_done(done_status); })));
  }

  // Saved report_request from mocked Transport::Report() call.
  ReportRequest report_request_;
  // If not NULL, the report response to send for mocked Transport::Report()
  // call.
  ReportResponse* report_response_;

  // saved on_done callback from either Transport::Check() or
  // Transport::Report().
  std::vector<TransportDoneFunc> on_done_vector_;
  // The status to send in on_done call back for Check() or Report().
  Status done_status_;
  // A vector to store thread objects used to call on_done callback.
  std::vector<std::unique_ptr<Thread>> callback_threads_;
};

// A mocking class to mock Periodic_Timer interface.
class MockPeriodicTimer {
 public:
  MOCK_METHOD2(StartTimer,
               std::unique_ptr<PeriodicTimer>(int, std::function<void()>));
  PeriodicTimerCreateFunc GetFunc() {
    return
        [this](int interval_ms,
               std::function<void()> func) -> std::unique_ptr<PeriodicTimer> {
          return this->StartTimer(interval_ms, func);
        };
  }

  class MockTimer : public PeriodicTimer {
   public:
    // Cancels the timer.
    MOCK_METHOD0(Stop, void());
  };

  std::unique_ptr<PeriodicTimer> MyStartTimer(int interval_ms,
                                              std::function<void()> callback) {
    interval_ms_ = interval_ms;
    callback_ = callback;
    return std::unique_ptr<PeriodicTimer>(new MockTimer);
  }

  int interval_ms_;
  std::function<void()> callback_;
};

}  // namespace

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_MOCK_TRANSPORT_H_
