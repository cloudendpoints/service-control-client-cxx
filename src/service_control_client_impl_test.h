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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_TEST_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_TEST_H_

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
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::_;

namespace google {
namespace service_control_client {
namespace {

const char kServiceName[] = "library.googleapis.com";
const char kServiceConfigId[] = "2016-09-19r0";

const char kCheckRequest1[] = R"(
service_name: "library.googleapis.com"
service_config_id: "2016-09-19r0"
operation {
  consumer_id: "project:some-consumer"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  operation_id: "operation-1"
  operation_name: "check-quota"
  metric_value_sets {
    metric_name: "serviceruntime.googleapis.com/api/consumer/quota_used_count"
    metric_values {
      labels {
        key: "/quota_group_name"
        value: "ReadGroup"
      }
      int64_value: 1000
    }
  }
}
)";

const char kSuccessCheckResponse1[] = R"(
operation_id: "operation-1"
)";

const char kErrorCheckResponse1[] = R"(
operation_id: "operation-1"
check_errors {
  code: PERMISSION_DENIED
  detail: "permission denied"
}
)";

const char kCheckRequest2[] = R"(
service_name: "library.googleapis.com"
service_config_id: "2016-09-19r0"
operation {
  consumer_id: "project:some-consumer"
  operation_id: "operation-2"
  operation_name: "check-quota-2"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  metric_value_sets {
    metric_name: "serviceruntime.googleapis.com/api/consumer/quota_used_count"
    metric_values {
      labels {
        key: "/quota_group_name"
        value: "ReadGroup"
      }
      int64_value: 2000
    }
  }
}
)";

const char kSuccessCheckResponse2[] = R"(
operation_id: "operation-2"
)";

const char kErrorCheckResponse2[] = R"(
operation_id: "operation-2"
check_errors {
  code: PERMISSION_DENIED
  detail: "permission denied"
}
)";

const char kReportRequest1[] = R"(
service_name: "library.googleapis.com"
service_config_id: "2016-09-19r0"
operations: {
  operation_id: "operation-1"
  consumer_id: "project:some-consumer"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  end_time {
    seconds: 3000
    nanos: 4000
  }
  log_entries {
    timestamp {
      seconds: 700
      nanos: 600
    }
    severity: INFO
    name: "system_event"
    text_payload: "Sample text log message 0"
  }
  metric_value_sets {
    metric_name: "library.googleapis.com/rpc/client/count"
    metric_values {
      start_time {
        seconds: 100
      }
      end_time {
        seconds: 300
      }
      int64_value: 1000
    }
  }
}
)";

const char kReportRequest2[] = R"(
service_name: "library.googleapis.com"
service_config_id: "2016-09-19r0"
operations: {
   operation_id: "operation-2"
  consumer_id: "project:some-consumer"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  end_time {
    seconds: 3000
    nanos: 4000
  }
  log_entries {
    timestamp {
      seconds: 700
      nanos: 600
    }
    severity: INFO
    name: "system_event"
    text_payload: "Sample text log message 1"
  }
  metric_value_sets {
    metric_name: "library.googleapis.com/rpc/client/count"
    metric_values {
      start_time {
        seconds: 200
      }
      end_time {
        seconds: 400
      }
      int64_value: 2000
    }
  }
}
)";

// Result of Merging request 1 into request 2, assuming they have delta metrics.
const char kReportDeltaMerged12[] = R"(
service_name: "library.googleapis.com"
service_config_id: "2016-09-19r0"
operations: {
  operation_id: "operation-1"
  consumer_id: "project:some-consumer"
  start_time {
    seconds: 1000
    nanos: 2000
  }
  end_time {
    seconds: 3000
    nanos: 4000
  }
  metric_value_sets {
    metric_name: "library.googleapis.com/rpc/client/count"
    metric_values {
      start_time {
        seconds: 100
      }
      end_time {
        seconds: 400
      }
      int64_value: 3000
    }
  }
  log_entries {
    severity: INFO
    timestamp {
      seconds: 700
      nanos: 600
    }
    text_payload: "Sample text log message 0"
    name: "system_event"
  }
  log_entries {
    severity: INFO
    timestamp {
      seconds: 700
      nanos: 600
    }
    text_payload: "Sample text log message 1"
    name: "system_event"
  }
}
)";

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

class ServiceControlClientImplTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kCheckRequest1, &check_request1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kSuccessCheckResponse1,
                                            &pass_check_response1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kErrorCheckResponse1,
                                            &error_check_response1_));

    ASSERT_TRUE(TextFormat::ParseFromString(kCheckRequest2, &check_request2_));
    ASSERT_TRUE(TextFormat::ParseFromString(kSuccessCheckResponse2,
                                            &pass_check_response2_));
    ASSERT_TRUE(TextFormat::ParseFromString(kErrorCheckResponse2,
                                            &error_check_response2_));

    ASSERT_TRUE(
        TextFormat::ParseFromString(kReportRequest1, &report_request1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kReportRequest2, &report_request2_));
    ASSERT_TRUE(TextFormat::ParseFromString(kReportDeltaMerged12,
                                            &merged_report_request_));

    ServiceControlClientOptions options(
        CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                                1000 /* expiration_ms */),
        QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
        ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));
    options.check_transport = mock_check_transport_.GetFunc();
    options.report_transport = mock_report_transport_.GetFunc();
    client_ =
        CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  }

  // Tests non cached check request. Mocked transport::Check() is storing
  // on_done() callback and call it in a delayed fashion within the same thread.
  // 1) Call a Client::Check(),  the request is not in the cache.
  // 2) Transport::Check() is called. Mocked transport::Check() stores
  //    the on_done callback.
  // 3) Client::Check() returns.  Client::on_check_done() is not called yet.
  // 4) Transport::on_done() is called in the same thread.
  // 5) Client::on_check_done() is called.
  void InternalTestNonCachedCheckWithStoredCallback(
      const CheckRequest& request, Status transport_status,
      CheckResponse* transport_response) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckWithStoredCallback));

    // Set the check response.
    mock_check_transport_.check_response_ = transport_response;
    size_t saved_done_vector_size =
        mock_check_transport_.on_done_vector_.size();

    CheckResponse check_response;
    Status done_status = Status::UNKNOWN;
    client_->Check(request, &check_response,
                   [&done_status](Status status) { done_status = status; });
    // on_check_done is not called yet. waiting for transport one_check_done.
    EXPECT_EQ(done_status, Status::UNKNOWN);

    // Since it is not cached, transport should be called.
    EXPECT_EQ(mock_check_transport_.on_done_vector_.size(),
              saved_done_vector_size + 1);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request));

    // Calls the on_check_done() to send status.
    mock_check_transport_.on_done_vector_[saved_done_vector_size](
        transport_status);
    // on_check_done is called with right status.
    EXPECT_EQ(done_status, transport_status);
    if (done_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Tests non cached check request. Mocked transport::Check() is called
  // right away (in place).
  // 1) Call a Client::Check(),  the request is not in the cache.
  // 2) Transport::Check() is called. on_done callback is called inside
  //    Transport::Check().
  void InternalTestNonCachedCheckWithInplaceCallback(
      const CheckRequest& request, Status transport_status,
      CheckResponse* transport_response) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckWithInplaceCallback));

    // Set the check status and response to be used in the on_check_done
    mock_check_transport_.done_status_ = transport_status;
    mock_check_transport_.check_response_ = transport_response;

    CheckResponse check_response;
    Status done_status = Status::UNKNOWN;
    client_->Check(request, &check_response,
                   [&done_status](Status status) { done_status = status; });
    // on_check_done should be called.
    EXPECT_EQ(done_status, transport_status);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request));
    if (transport_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  void InternalTestNonCachedBlockingCheckWithInplaceCallback(
      const CheckRequest& request, Status transport_status,
      CheckResponse* transport_response) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckWithInplaceCallback));

    // Set the check status and response to be used in the on_check_done
    mock_check_transport_.done_status_ = transport_status;
    mock_check_transport_.check_response_ = transport_response;

    CheckResponse check_response;
    Status done_status = client_->Check(request, &check_response);

    EXPECT_EQ(done_status, transport_status);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request));
    if (transport_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expectations and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Tests non cached check request. Mocked transport::Check() is using a
  // separate thread to call on_done callback.
  // 1) Call a Client::Check(),  the request is not in the cache.
  // 2) Transport::Check() is called. Mocked transport::Check() creates a
  //    separate thread to call on_done callback.
  // 3) Client::Check() returns, but Client::on_check_done() will be called
  //    from the other thread.
  void InternalTestNonCachedCheckUsingThread(
      const CheckRequest& request, Status transport_status,
      CheckResponse* transport_response) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckUsingThread));

    // Set the check status and response to be used in the on_check_done
    mock_check_transport_.done_status_ = transport_status;
    mock_check_transport_.check_response_ = transport_response;

    StatusPromise status_promise;
    StatusFuture status_future = status_promise.get_future();

    CheckResponse check_response;
    client_->Check(request, &check_response, [&status_promise](Status status) {
      StatusPromise moved_promise(std::move(status_promise));
      moved_promise.set_value(status);
    });

    // Since it is not cached, transport should be called.
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request));

    // on_check_done is called with right status.
    status_future.wait();
    EXPECT_EQ(transport_status, status_future.get());
    if (transport_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  void InternalTestNonCachedBlockingCheckUsingThread(
      const CheckRequest& request, Status transport_status,
      CheckResponse* transport_response) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckUsingThread));

    // Set the check status and response to be used in the on_check_done
    mock_check_transport_.done_status_ = transport_status;
    mock_check_transport_.check_response_ = transport_response;

    CheckResponse check_response;
    // Test with blocking check.
    Status done_status = client_->Check(request, &check_response);

    // Since it is not cached, transport should be called.
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request));

    EXPECT_EQ(transport_status, done_status);
    if (transport_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expectations and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Before this call, cache should have request1. This test will call Check
  // with request2, and it calls Transport::Check() and get a good
  // response2 and set it to cache.  This will evict the request1.  The
  // evicted request1 will be called Transport::Check() again, and its response
  // is dropped. The cache will have request2.
  void InternalTestReplacedGoodCheckWithStoredCallback(
      const CheckRequest& request2, Status transport_status2,
      CheckResponse* transport_response2, const CheckRequest& request1,
      Status transport_status1, CheckResponse* transport_response1) {
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckWithStoredCallback));

    // Set the check response.
    mock_check_transport_.check_response_ = transport_response2;
    size_t saved_done_vector_size =
        mock_check_transport_.on_done_vector_.size();

    CheckResponse check_response2;
    Status done_status2 = Status::UNKNOWN;
    client_->Check(request2, &check_response2,
                   [&done_status2](Status status) { done_status2 = status; });
    // on_check_done is not called yet. waiting for transport one_check_done.
    EXPECT_EQ(done_status2, Status::UNKNOWN);

    // Since it is not cached, transport should be called.
    EXPECT_EQ(mock_check_transport_.on_done_vector_.size(),
              saved_done_vector_size + 1);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request2));

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));

    // Once on_done_ is called, it will call CacheResponse
    // which evicts out the old item. The evicted item will call
    // Transport::Check.
    EXPECT_CALL(mock_check_transport_, Check(_, _, _))
        .WillOnce(Invoke(&mock_check_transport_,
                         &MockCheckTransport::CheckWithStoredCallback));

    // Set the check response for the next request
    mock_check_transport_.check_response_ = transport_response1;

    // Calls the on_check_done() to send status.
    mock_check_transport_.on_done_vector_[saved_done_vector_size](
        transport_status2);
    // on_check_done is called with right status.
    EXPECT_EQ(done_status2, transport_status2);
    EXPECT_TRUE(
        MessageDifferencer::Equals(*transport_response2, check_response2));

    // request1 should be evited out, and called Transport.
    EXPECT_EQ(mock_check_transport_.on_done_vector_.size(),
              saved_done_vector_size + 2);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_check_transport_.check_request_,
                                           request1));

    // Calls the on_check_done() to send status.
    mock_check_transport_.on_done_vector_[saved_done_vector_size + 1](
        transport_status1);
    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Before this call, cache should have request1. This test will call Check
  // with request2, and it calls Transport::Check() and get a good
  // response2 and set it to cache.  This will evict the request1.  The
  // evicted request1 will be called Transport::Check() again, and its response
  // is dropped. The cache will have request2.
  void InternalTestReplacedGoodCheckWithInplaceCallback(
      const CheckRequest& request2, Status transport_status2,
      CheckResponse* transport_response2) {
    // Transport::Check() will be called twice. First one is for request2
    // The second one is for evicted request1.
    ON_CALL(mock_check_transport_, Check(_, _, _))
        .WillByDefault(Invoke(&mock_check_transport_,
                              &MockCheckTransport::CheckWithInplaceCallback));
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(2);

    // Both requests will use the same status and response.
    mock_check_transport_.done_status_ = transport_status2;
    mock_check_transport_.check_response_ = transport_response2;

    CheckResponse check_response;
    Status done_status = Status::UNKNOWN;
    client_->Check(request2, &check_response,
                   [&done_status](Status status) { done_status = status; });
    EXPECT_EQ(transport_status2, done_status);
    if (transport_status2.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response2, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  void InternalTestReplacedBlockingCheckWithInplaceCallback(
      const CheckRequest& request2, Status transport_status2,
      CheckResponse* transport_response2) {
    // Transport::Check() will be called twice. First one is for request2
    // The second one is for evicted request1.
    ON_CALL(mock_check_transport_, Check(_, _, _))
        .WillByDefault(Invoke(&mock_check_transport_,
                              &MockCheckTransport::CheckWithInplaceCallback));
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(2);

    // Both requests will use the same status and response.
    mock_check_transport_.done_status_ = transport_status2;
    mock_check_transport_.check_response_ = transport_response2;

    CheckResponse check_response;
    // Test with blocking check.
    Status done_status = client_->Check(request2, &check_response);
    EXPECT_EQ(transport_status2, done_status);
    if (transport_status2.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response2, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Before this call, cache should have request1. This test will call Check
  // with request2, and it calls Transport::Check() and get a good
  // response2 and set it to cache.  This will evict the request1.  The
  // evicted request1 will be called Transport::Check() again, and its response
  // is dropped. The cache will have request2.
  void InternalTestReplacedGoodCheckUsingThread(
      const CheckRequest& request2, Status transport_status2,
      CheckResponse* transport_response2) {
    // Transport::Check() will be called twice. First one is for request2
    // The second one is for evicted request1.
    ON_CALL(mock_check_transport_, Check(_, _, _))
        .WillByDefault(Invoke(&mock_check_transport_,
                              &MockCheckTransport::CheckUsingThread));
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(2);

    // Both requests will use the same status and response.
    mock_check_transport_.done_status_ = transport_status2;
    mock_check_transport_.check_response_ = transport_response2;

    StatusPromise status_promise;
    StatusFuture status_future = status_promise.get_future();

    CheckResponse check_response;
    client_->Check(request2, &check_response, [&status_promise](Status status) {
      StatusPromise moved_promise(std::move(status_promise));
      moved_promise.set_value(status);
    });

    // on_check_done is called with right status.
    status_future.wait();
    EXPECT_EQ(transport_status2, status_future.get());
    if (transport_status2.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response2, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  void InternalTestReplacedBlockingCheckUsingThread(
      const CheckRequest& request2, Status transport_status2,
      CheckResponse* transport_response2) {
    // Test with blocking check.
    // Transport::Check() will be called twice. First one is for request2
    // The second one is for evicted request1.
    ON_CALL(mock_check_transport_, Check(_, _, _))
        .WillByDefault(Invoke(&mock_check_transport_,
                              &MockCheckTransport::CheckUsingThread));
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(2);

    // Both requests will use the same status and response.
    mock_check_transport_.done_status_ = transport_status2;
    mock_check_transport_.check_response_ = transport_response2;

    CheckResponse check_response;
    // Test with blocking check.
    Status done_status = client_->Check(request2, &check_response);

    EXPECT_EQ(transport_status2, done_status);
    if (transport_status2.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response2, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Tests a cached check request.
  // 1) Calls a Client::Check(), its request is in the cache.
  // 2) Client::on_check_done() is called right away.
  // 3) Transport::Check() is not called.
  void InternalTestCachedCheck(const CheckRequest& request,
                               const CheckResponse& expected_response) {
    // Check should not be called with cached entry
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(0);

    CheckResponse cached_response;
    Status cached_done_status = Status::UNKNOWN;
    client_->Check(
        request, &cached_response,
        [&cached_done_status](Status status) { cached_done_status = status; });
    // on_check_done is called inplace with a cached entry.
    EXPECT_OK(cached_done_status);
    EXPECT_TRUE(MessageDifferencer::Equals(expected_response, cached_response));

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  void InternalTestCachedBlockingCheck(const CheckRequest& request,
                                       const CheckResponse& expected_response) {
    // Check should not be called with cached entry
    EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(0);

    CheckResponse cached_response;
    // Test with blocking check.
    Status cached_done_status = client_->Check(request, &cached_response);
    // on_check_done is called inplace with a cached entry.
    EXPECT_OK(cached_done_status);
    EXPECT_TRUE(MessageDifferencer::Equals(expected_response, cached_response));

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_check_transport_));
  }

  // Adds a label to the given operation.
  void AddLabel(const string& key, const string& value, Operation* operation) {
    (*operation->mutable_labels())[key] = value;
  }

  CheckRequest check_request1_;
  CheckResponse pass_check_response1_;
  CheckResponse error_check_response1_;

  CheckRequest check_request2_;
  CheckResponse pass_check_response2_;
  CheckResponse error_check_response2_;

  ReportRequest report_request1_;
  ReportRequest report_request2_;
  ReportRequest merged_report_request_;

  MockCheckTransport mock_check_transport_;
  MockReportTransport mock_report_transport_;
  std::unique_ptr<ServiceControlClient> client_;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_IMPL_TEST_H_
