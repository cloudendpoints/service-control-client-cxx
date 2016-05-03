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

const char kCheckRequest1[] = R"(
service_name: "library.googleapis.com"
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
  code: LOAD_SHEDDING
  detail: "load shedding"
}
check_errors {
  code: ABUSER_DETECTED
  detail: "abuse detected"
}
)";

const char kCheckRequest2[] = R"(
service_name: "library.googleapis.com"
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
  code: LOAD_SHEDDING
  detail: "load shedding"
}
check_errors {
  code: ABUSER_DETECTED
  detail: "abuse detected"
}
)";

const char kReportRequest1[] = R"(
service_name: "library.googleapis.com"
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
    metadata {
      timestamp {
        seconds: 700
        nanos: 600
      }
      severity: INFO
    }
    log: "system_event"
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
    metadata {
      timestamp {
        seconds: 700
        nanos: 600
      }
      severity: INFO
    }
    log: "system_event"
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
    metadata {
      severity: INFO
      timestamp {
        seconds: 700
        nanos: 600
      }
    }
    text_payload: "Sample text log message 0"
    log: "system_event"
  }
  log_entries {
    metadata {
      severity: INFO
      timestamp {
        seconds: 700
        nanos: 600
      }
    }
    text_payload: "Sample text log message 1"
    log: "system_event"
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
        ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));
    options.check_transport = mock_check_transport_.GetFunc();
    options.report_transport = mock_report_transport_.GetFunc();
    client_ = std::move(CreateServiceControlClient(kServiceName, options));
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
      status_promise.set_value(status);
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
      status_promise.set_value(status);
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

TEST_F(ServiceControlClientImplTest, TestNonCachedCheckWithStoredCallback) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called.  It will send a successful check response
  // The response should be stored in the cache.
  // Client::Check is called with the same check request. It should use the one
  // in the cache. Such call did not change the cache state, it can be called
  // repeatly.
  InternalTestNonCachedCheckWithStoredCallback(check_request1_, Status::OK,
                                               &pass_check_response1_);
  // For a cached request, it can be called repeatedly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedCheck(check_request1_, pass_check_response1_);
  }
  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_checks, 11);
  EXPECT_EQ(stat.send_checks_by_flush, 0);
  EXPECT_EQ(stat.send_checks_in_flight, 1);
  EXPECT_EQ(stat.send_report_operations, 0);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest, TestReplacedGoodCheckWithStoredCallback) {
  // Send request1 and a pass response to cache,
  // then replace it with request2.  request1 will be evited, it will be send
  // to server again.
  InternalTestNonCachedCheckWithStoredCallback(check_request1_, Status::OK,
                                               &pass_check_response1_);
  InternalTestCachedCheck(check_request1_, pass_check_response1_);
  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_checks, 2);
  EXPECT_EQ(stat.send_checks_by_flush, 0);
  EXPECT_EQ(stat.send_checks_in_flight, 1);
  EXPECT_EQ(stat.send_report_operations, 0);

  InternalTestReplacedGoodCheckWithStoredCallback(
      check_request2_, Status::OK, &pass_check_response2_, check_request1_,
      Status::OK, &pass_check_response1_);
  InternalTestCachedCheck(check_request2_, pass_check_response2_);
  stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_checks, 4);
  EXPECT_EQ(stat.send_checks_by_flush, 1);
  EXPECT_EQ(stat.send_checks_in_flight, 2);
  EXPECT_EQ(stat.send_report_operations, 0);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest, TestReplacedBadCheckWithStoredCallback) {
  // Send request1 and a error response to cache,
  // then replace it with request2.  request1 will be evited. Since it only
  // has an error response, it will not need to sent to server
  InternalTestNonCachedCheckWithStoredCallback(check_request1_, Status::OK,
                                               &error_check_response1_);
  InternalTestCachedCheck(check_request1_, error_check_response1_);

  InternalTestNonCachedCheckWithStoredCallback(check_request2_, Status::OK,
                                               &error_check_response2_);
  InternalTestCachedCheck(check_request2_, error_check_response2_);
}

TEST_F(ServiceControlClientImplTest,
       TestFailedNonCachedCheckWithStoredCallback) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called, but it failed with PERMISSION_DENIED error.
  // The response is not cached.
  // Such call did not change cache state, it can be called repeatly.

  // For a failed Check calls, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestNonCachedCheckWithStoredCallback(
        check_request1_, Status(Code::PERMISSION_DENIED, ""),
        &pass_check_response1_);
  }
}

TEST_F(ServiceControlClientImplTest,
       TestNonCachedCheckWithStoredCallbackWithPerRequestTransport) {
  MockCheckTransport stack_mock_check_transport;
  EXPECT_CALL(stack_mock_check_transport, Check(_, _, _))
      .WillOnce(Invoke(&stack_mock_check_transport,
                       &MockCheckTransport::CheckWithStoredCallback));

  stack_mock_check_transport.check_response_ = &pass_check_response1_;

  CheckResponse check_response;
  Status done_status = Status::UNKNOWN;
  client_->Check(check_request1_, &check_response,
                 [&done_status](Status status) { done_status = status; },
                 stack_mock_check_transport.GetFunc());
  // on_check_done is not called yet. waiting for transport one_check_done.
  EXPECT_EQ(done_status, Status::UNKNOWN);

  // Since it is not cached, transport should be called.
  EXPECT_EQ(stack_mock_check_transport.on_done_vector_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(
      stack_mock_check_transport.check_request_, check_request1_));

  // Calls the on_check_done() to send status.
  stack_mock_check_transport.on_done_vector_[0](Status::OK);
  // on_check_done is called with right status.
  EXPECT_TRUE(done_status.ok());
  EXPECT_TRUE(
      MessageDifferencer::Equals(check_response, pass_check_response1_));

  // Verifies call expections and clear it before other test.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&stack_mock_check_transport));

  // For a cached request, it can be called repeatedly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedCheck(check_request1_, pass_check_response1_);
  }

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedCheckWithInplaceCallback) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called.  It will send a successful check response
  // The response should be stored in the cache.
  // Client::Check is called with the same check request. It should use the one
  // in the cache. Such call did not change the cache state, it can be called
  // repeatly.
  InternalTestNonCachedCheckWithInplaceCallback(check_request1_, Status::OK,
                                                &pass_check_response1_);
  // For a cached request, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedCheck(check_request1_, pass_check_response1_);
  }

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest,
       TestNonCachedBlockingCheckWithInplaceCallback) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called.  It will send a successful check response
  // The response should be stored in the cache.
  // Client::Check is called with the same check request. It should use the one
  // in the cache. Such call did not change the cache state, it can be called
  // repeatly.
  // Test with blocking check.
  InternalTestNonCachedBlockingCheckWithInplaceCallback(
      check_request1_, Status::OK, &pass_check_response1_);
  // For a cached request, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedBlockingCheck(check_request1_, pass_check_response1_);
  }

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckWithInplaceCallback));
}

TEST_F(ServiceControlClientImplTest, TestReplacedGoodCheckWithInplaceCallback) {
  // Send request1 and a pass response to cache,
  // then replace it with request2.  request1 will be evited, it will be send
  // to server again.
  InternalTestNonCachedCheckWithInplaceCallback(check_request1_, Status::OK,
                                                &pass_check_response1_);
  InternalTestCachedCheck(check_request1_, pass_check_response1_);

  InternalTestReplacedGoodCheckWithInplaceCallback(check_request2_, Status::OK,
                                                   &pass_check_response2_);
  InternalTestCachedCheck(check_request2_, pass_check_response2_);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest,
       TestReplacedBlockingCheckWithInplaceCallback) {
  // Send request1 and a pass response to cache,
  // then replace it with request2.  request1 will be evited, it will be send
  // to server again.
  // Test with blocking check.
  InternalTestNonCachedBlockingCheckWithInplaceCallback(
      check_request1_, Status::OK, &pass_check_response1_);
  InternalTestCachedBlockingCheck(check_request1_, pass_check_response1_);

  InternalTestReplacedBlockingCheckWithInplaceCallback(
      check_request2_, Status::OK, &pass_check_response2_);
  InternalTestCachedBlockingCheck(check_request2_, pass_check_response2_);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckWithInplaceCallback));
}

TEST_F(ServiceControlClientImplTest, TestReplacedBadCheckWithInplaceCallback) {
  // Send request1 and a error response to cache,
  // then replace it with request2.  request1 will be evited. Since it only
  // has an error response, it will not need to sent to server
  InternalTestNonCachedCheckWithInplaceCallback(check_request1_, Status::OK,
                                                &error_check_response1_);
  InternalTestCachedCheck(check_request1_, error_check_response1_);

  InternalTestNonCachedCheckWithInplaceCallback(check_request2_, Status::OK,
                                                &error_check_response2_);
  InternalTestCachedCheck(check_request2_, error_check_response2_);
}

TEST_F(ServiceControlClientImplTest,
       TestFailedNonCachedCheckWithInplaceCallback) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called, but it failed with PERMISSION_DENIED error.
  // The response is not cached.
  // Such call did not change cache state, it can be called repeatly.

  // For a failed Check calls, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestNonCachedCheckWithInplaceCallback(
        check_request1_, Status(Code::PERMISSION_DENIED, ""),
        &pass_check_response1_);
  }
}

TEST_F(ServiceControlClientImplTest, TestNonCachedCheckUsingThread) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called.  It will send an error check response
  // The response should be stored in the cache.
  // Client::Check is called with the same check request. It should use the one
  // in the cache. Such call did not change the cache state, it can be called
  // repeatly.
  InternalTestNonCachedCheckUsingThread(check_request1_, Status::OK,
                                        &error_check_response1_);
  // For a cached request, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedCheck(check_request1_, error_check_response1_);
  }

  // Since the cache response is an error response, when it is removed from the
  // cache, it doesn't need to send to server. So transport is not called.
}

TEST_F(ServiceControlClientImplTest, TestNonCachedBlockingCheckUsingThread) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called.  It will send an error check response
  // The response should be stored in the cache.
  // Client::Check is called with the same check request. It should use the one
  // in the cache. Such call did not change the cache state, it can be called
  // repeatedly.
  // Test with blocking check.
  InternalTestNonCachedBlockingCheckUsingThread(check_request1_, Status::OK,
                                                &error_check_response1_);
  // For a cached request, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedBlockingCheck(check_request1_, error_check_response1_);
  }

  // Since the cache response is an error response, when it is removed from the
  // cache, it doesn't need to send to server. So transport is not called.
}

TEST_F(ServiceControlClientImplTest, TestReplacedGoodCheckUsingThread) {
  // Send request1 and a pass response to cache,
  // then replace it with request2.  request1 will be evited, it will be send
  // to server again.
  InternalTestNonCachedCheckUsingThread(check_request1_, Status::OK,
                                        &pass_check_response1_);
  InternalTestCachedCheck(check_request1_, pass_check_response1_);

  InternalTestReplacedGoodCheckUsingThread(check_request2_, Status::OK,
                                           &pass_check_response2_);
  InternalTestCachedCheck(check_request2_, pass_check_response2_);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest, TestReplacedBlockingCheckUsingThread) {
  // Send request1 and a pass response to cache,
  // then replace it with request2.  request1 will be evited, it will be send
  // to server again.
  // Test with blocking check:
  InternalTestNonCachedBlockingCheckUsingThread(check_request1_, Status::OK,
                                                &pass_check_response1_);
  InternalTestCachedBlockingCheck(check_request1_, pass_check_response1_);

  InternalTestReplacedBlockingCheckUsingThread(check_request2_, Status::OK,
                                               &pass_check_response2_);
  InternalTestCachedBlockingCheck(check_request2_, pass_check_response2_);

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckUsingThread));
}

TEST_F(ServiceControlClientImplTest, TestReplacedBadCheckUsingThread) {
  // Send request1 and a error response to cache,
  // then replace it with request2.  request1 will be evited. Since it only
  // has an error response, it will not need to sent to server
  InternalTestNonCachedCheckUsingThread(check_request1_, Status::OK,
                                        &error_check_response1_);
  InternalTestCachedCheck(check_request1_, error_check_response1_);

  InternalTestNonCachedCheckUsingThread(check_request2_, Status::OK,
                                        &error_check_response2_);
  InternalTestCachedCheck(check_request2_, error_check_response2_);
}

TEST_F(ServiceControlClientImplTest, TestFailedNonCachedCheckUsingThread) {
  // Calls a Client::Check, the request is not in the cache
  // Transport::Check() is called, but it failed with PERMISSION_DENIED error.
  // The response is not cached.
  // Such call did not change cache state, it can be called repeatly.

  // For a failed Check calls, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestNonCachedCheckUsingThread(check_request1_,
                                          Status(Code::PERMISSION_DENIED, ""),
                                          &pass_check_response1_);
  }
}

TEST_F(ServiceControlClientImplTest, TestCachedReportWithStoredCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2, it should be cached.
  // Transport::Report() should not be called.
  // After client is destroyed, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));
  // Only after client is destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         merged_report_request_));

  // Call the on_check_done() to complete the data flow.
  mock_report_transport_.on_done_vector_[0](Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportWithInplaceCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2, it should be cached.
  // Transport::Report() should not be called.
  // After client destroyed, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         merged_report_request_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportUsingThread) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2, it should be cached.
  // Transport::Report() should not be called.
  // After client destroyed, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         merged_report_request_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportWithStoredCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After client destroyed, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  EXPECT_OK(done_status2);

  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  mock_report_transport_.on_done_vector_[0](Status::OK);
  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 2);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request2_));

  // Call the on_check_done() to complete the data flow.
  mock_report_transport_.on_done_vector_[1](Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportWithInplaceCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After client destroyed, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest,
       TestReplacedBlockingReportWithInplaceCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After client destroyed, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  // Test with blocking Report.
  Status done_status1 = client_->Report(report_request1_, &report_response);
  EXPECT_OK(done_status1);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));

  Status done_status2 = client_->Report(report_request2_, &report_response);
  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportUsingThread) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After client destroyed, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_reports, 1);
  EXPECT_EQ(stat.send_reports_by_flush, 0);
  EXPECT_EQ(stat.send_reports_in_flight, 0);
  EXPECT_EQ(stat.send_report_operations, 0);

  EXPECT_OK(done_status1);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request2_, &report_response,
                  [&done_status2](Status status) { done_status2 = status; });
  stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_reports, 2);
  EXPECT_EQ(stat.send_reports_by_flush, 1);
  EXPECT_EQ(stat.send_reports_in_flight, 0);
  EXPECT_EQ(stat.send_report_operations, 1);

  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedBlockingReportUsingThread) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After client destroyed, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = client_->Report(report_request1_, &report_response);
  EXPECT_OK(done_status1);

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));
  // Test with blocking Report.
  Status done_status2 = client_->Report(report_request2_, &report_response);
  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // Verifies that mock_report_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));

  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));
  // Only after client destroyed, mock_report_transport_::Report() is called.
  client_.reset();
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportWithStoredCallback) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in the same thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));

  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(report_request1_, &report_response,
                  [&done_status](Status status) { done_status = status; });
  // on_report_done is not called yet. waiting for transport one_report_done.
  EXPECT_EQ(done_status, Status::UNKNOWN);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // Calls the on_check_done() to send status.
  mock_report_transport_.on_done_vector_[0](
      Status(Code::PERMISSION_DENIED, ""));
  // on_report_done is called with right status.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);
}

TEST_F(ServiceControlClientImplTest,
       TestNonCachedReportWithStoredCallbackWithPerRequestTransport) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in the same thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
  MockReportTransport stack_mock_report_transport;
  EXPECT_CALL(stack_mock_report_transport, Report(_, _, _))
      .WillOnce(Invoke(&stack_mock_report_transport,
                       &MockReportTransport::ReportWithStoredCallback));

  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(report_request1_, &report_response,
                  [&done_status](Status status) { done_status = status; },
                  stack_mock_report_transport.GetFunc());
  // on_report_done is not called yet. waiting for transport one_report_done.
  EXPECT_EQ(done_status, Status::UNKNOWN);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(stack_mock_report_transport.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(
      stack_mock_report_transport.report_request_, report_request1_));

  // Calls the on_check_done() to send status.
  stack_mock_report_transport.on_done_vector_[0](
      Status(Code::PERMISSION_DENIED, ""));
  // on_report_done is called with right status.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportWithInplaceCallback) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called inside Transport::Report() with error
  // PERMISSION_DENIED. The Client::done_done() is called with the same error.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));

  // Set the report status to be used in the on_report_done
  mock_report_transport_.done_status_ = Status(Code::PERMISSION_DENIED, "");

  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(report_request1_, &report_response,
                  [&done_status](Status status) { done_status = status; });

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_reports, 1);
  EXPECT_EQ(stat.send_reports_by_flush, 0);
  EXPECT_EQ(stat.send_reports_in_flight, 1);
  EXPECT_EQ(stat.send_report_operations, 1);

  // one_done should be called for now.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));
}

TEST_F(ServiceControlClientImplTest,
       TestNonCachedBlockingReportWithInplaceCallback) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called inside Transport::Report() with error
  // PERMISSION_DENIED. The Client::done_done() is called with the same error.
  // Test with Blocking Report.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithInplaceCallback));

  // Set the report status to be used in the on_report_done
  mock_report_transport_.done_status_ = Status(Code::PERMISSION_DENIED, "");

  ReportResponse report_response;

  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  // Test with Blocking Report.
  Status done_status = client_->Report(report_request1_, &report_response);

  // one_done should be called for now.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportUsingThread) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in a separate thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));

  // Set the report status to be used in the on_report_done
  mock_report_transport_.done_status_ = Status(Code::PERMISSION_DENIED, "");

  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  ReportResponse report_response;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(
      report_request1_, &report_response,
      [&status_promise](Status status) { status_promise.set_value(status); });

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);
  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_reports, 1);
  EXPECT_EQ(stat.send_reports_by_flush, 0);
  EXPECT_EQ(stat.send_reports_in_flight, 1);
  EXPECT_EQ(stat.send_report_operations, 1);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // on_report_done is called with right status.
  status_future.wait();
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, status_future.get());
}

TEST_F(ServiceControlClientImplTest, TestNonCachedBlockingReportUsingThread) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in a separate thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportUsingThread));

  // Set the report status to be used in the on_report_done
  mock_report_transport_.done_status_ = Status(Code::PERMISSION_DENIED, "");

  ReportResponse report_response;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  // Test with Blocking Report.
  Status done_status = client_->Report(report_request1_, &report_response);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));

  // on_report_done is called with right status.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);
}

TEST_F(ServiceControlClientImplTest, TestFlushIntervalReportNeverFlush) {
  // With periodic_timer, report flush interval is -1, Check flush interval is
  // 1000, so the overall flush interval is 1000
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      ReportAggregationOptions(1 /* entries */, -1 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(kServiceName, options));
  ASSERT_EQ(mock_timer.interval_ms_, 1000);
}

TEST_F(ServiceControlClientImplTest, TestFlushIntervalCheckNeverFlush) {
  // With periodic_timer, report flush interval is 500,
  // Check flush interval is -1 since its cache is disabled.
  // So the overall flush interval is 500
  ServiceControlClientOptions options(
      // If entries = 0, cache is disabled, GetNextFlushInterval() will be -1.
      CheckAggregationOptions(0 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(kServiceName, options));
  ASSERT_EQ(mock_timer.interval_ms_, 500);
}

TEST_F(ServiceControlClientImplTest, TestFlushInterval) {
  // With periodic_timer, report flush interval is 800, Check flush interval is
  // 1000, So the overall flush interval is 800
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      ReportAggregationOptions(1 /* entries */, 800 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(kServiceName, options));
  ASSERT_EQ(mock_timer.interval_ms_, 800);
}

TEST_F(ServiceControlClientImplTest, TestFlushCalled) {
  // To test flush function is called properly with periodic_timer.
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.report_transport = mock_report_transport_.GetFunc();
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  client_ = std::move(CreateServiceControlClient(kServiceName, options));
  ASSERT_TRUE(mock_timer.callback_ != NULL);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);
  // Wait for cached item to be expired.
  usleep(600000);
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));

  // client call Flush()
  mock_timer.callback_();

  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));
  // Call the on_check_done() to complete the data flow.
  mock_report_transport_.on_done_vector_[0](Status::OK);
}

TEST_F(ServiceControlClientImplTest,
       TestTimerCallbackCalledAfterClientDeleted) {
  // When the client object is deleted, timer callback may be called after it
  // is deleted,  it should not crash.
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.report_transport = mock_report_transport_.GetFunc();
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  client_ = std::move(CreateServiceControlClient(kServiceName, options));
  ASSERT_TRUE(mock_timer.callback_ != NULL);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(report_request1_, &report_response,
                  [&done_status1](Status status) { done_status1 = status; });
  EXPECT_OK(done_status1);

  // Only after client is destroyed, mock_report_transport_::Report() is called.
  EXPECT_CALL(mock_report_transport_, Report(_, _, _))
      .WillOnce(Invoke(&mock_report_transport_,
                       &MockReportTransport::ReportWithStoredCallback));
  client_.reset();

  EXPECT_TRUE(mock_report_transport_.on_done_vector_.size() == 1);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_report_transport_.report_request_,
                                         report_request1_));
  // Call the on_check_done() to complete the data flow.
  mock_report_transport_.on_done_vector_[0](Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_report_transport_));
  mock_timer.callback_();
}

}  // namespace service_control_client
}  // namespace google
