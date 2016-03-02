#include "include/service_control_client.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <future>
#include <thread>

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

class MockNonBlockingTransport : public NonBlockingTransport {
 public:
  MOCK_METHOD3(Check, void(const CheckRequest&, CheckResponse*, DoneCallback));
  MOCK_METHOD3(Report,
               void(const ReportRequest&, ReportResponse*, DoneCallback));

  MockNonBlockingTransport()
      : check_response_(NULL), report_response_(NULL), on_done_(NULL) {}

  ~MockNonBlockingTransport() override {
    if (callback_thread_) {
      callback_thread_->join();
    }
  }

  // The done callback is stored in on_done_.
  // It MUST be called later.
  void CheckWithStoredCallback(const CheckRequest& request,
                               CheckResponse* response, DoneCallback on_done) {
    check_request_ = request;
    if (check_response_) {
      *response = *check_response_;
    }
    on_done_ = on_done;
  }

  // The done callback is called right away (in place).
  // This will not work (pending bug b/27487040)
  // TODO(qians): please add InplaceCallback functions after fixing b/27487040
  void CheckWithInplaceCallback(const CheckRequest& request,
                                CheckResponse* response, DoneCallback on_done) {
    check_request_ = request;
    if (check_response_) {
      *response = *check_response_;
    }
    on_done(done_status_);
  }

  // The done callback is called from a separate thread with check_status_
  void CheckUsingThread(const CheckRequest& request, CheckResponse* response,
                        DoneCallback on_done) {
    check_request_ = request;
    callback_thread_.reset(new std::thread([
      on_done, done_status = done_status_, check_response = check_response_,
      response
    ]() {
      if (check_response) {
        *response = *check_response;
      }
      on_done(done_status);
    }));
  }

  // The done callback is stored in on_done_.
  // It MUST be called later.
  void ReportWithStoredCallback(const ReportRequest& request,
                                ReportResponse* response,
                                DoneCallback on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    on_done_ = on_done;
  }

  // The done callback is called right away (in place).
  void ReportWithInplaceCallback(const ReportRequest& request,
                                 ReportResponse* response,
                                 DoneCallback on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    on_done(done_status_);
  }

  // The done callback is called from a separate thread with done_status_
  void ReportUsingThread(const ReportRequest& request, ReportResponse* response,
                         DoneCallback on_done) {
    report_request_ = request;
    if (report_response_) {
      *response = *report_response_;
    }
    callback_thread_.reset(new std::thread(
        [ on_done, done_status = done_status_ ]() { on_done(done_status); }));
  }

  CheckRequest check_request_;
  CheckResponse* check_response_;
  ReportRequest report_request_;
  ReportResponse* report_response_;

  DoneCallback on_done_;
  Status done_status_;
  std::unique_ptr<std::thread> callback_thread_;
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
    ASSERT_TRUE(
        TextFormat::ParseFromString(kReportRequest1, &report_request1_));

    ReportAggregationOptions report_options(1 /* entries */,
                                            500 /*flush_interval_ms*/);
    CheckAggregationOptions check_options(1 /*entries */,
                                          500 /* refresh_interval_ms */,
                                          1000 /* expiration_ms */);

    client_ = std::move(CreateServiceControlClient(
        kServiceName, check_options, report_options,
        std::shared_ptr<MetricKindMap>(new MetricKindMap)));
    mock_transport_ = new MockNonBlockingTransport;
    client_->SetNonBlockingTransport(
        std::unique_ptr<NonBlockingTransport>(mock_transport_));
  }

  void InternalTestNonCachedCheckWithStoredCallback(
      const CheckRequest& request, Status call_status,
      CheckResponse* call_response) {
    EXPECT_CALL(*mock_transport_, Check(_, _, _))
        .WillOnce(Invoke(mock_transport_,
                         &MockNonBlockingTransport::CheckWithStoredCallback));

    // Set the check response.
    mock_transport_->check_response_ = call_response;

    CheckResponse check_response;
    Status done_status = Status::UNKNOWN;
    client_->Check(
        request, &check_response,
        [& ret_status = done_status](Status status) { ret_status = status; });
    // on_check_done is not called yet. waiting for transport one_check_done.
    EXPECT_EQ(done_status, Status::UNKNOWN);

    // Since it is not cached, transport should be called.
    EXPECT_TRUE(mock_transport_->on_done_ != NULL);
    EXPECT_TRUE(
        MessageDifferencer::Equals(mock_transport_->check_request_, request));

    // Calls the on_check_done() to send status.
    mock_transport_->on_done_(call_status);
    // on_check_done is called with right status.
    EXPECT_EQ(done_status, call_status);
    if (done_status.ok()) {
      EXPECT_TRUE(MessageDifferencer::Equals(*call_response, check_response));
    }

    // Verifies call expections and clear it before following cache test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  }

  void InternalTestNonCachedCheckUsingThread(const CheckRequest& request,
                                             Status call_status,
                                             CheckResponse* call_response) {
    EXPECT_CALL(*mock_transport_, Check(_, _, _))
        .WillOnce(Invoke(mock_transport_,
                         &MockNonBlockingTransport::CheckUsingThread));

    // Set the check status and response to be used in the on_check_done
    mock_transport_->done_status_ = call_status;
    mock_transport_->check_response_ = call_response;

    std::promise<Status> status_promise;
    std::future<Status> status_future = status_promise.get_future();

    CheckResponse check_response;
    client_->Check(request, &check_response,
                   [& promise = status_promise](Status status) {
                     promise.set_value(status);
                   });

    // Since it is not cached, transport should be called.
    EXPECT_TRUE(
        MessageDifferencer::Equals(mock_transport_->check_request_, request));

    // on_check_done is called with right status.
    status_future.wait();
    EXPECT_EQ(call_status, status_future.get());
    if (call_status.ok()) {
      EXPECT_TRUE(MessageDifferencer::Equals(*call_response, check_response));
    }

    // Verifies call expections and clear it before following cache test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

    // Make sure to release the thread object.
    if (mock_transport_->callback_thread_) {
      mock_transport_->callback_thread_->join();
      mock_transport_->callback_thread_.reset(NULL);
    }
  }

  void InternalTestCachedCheck(const CheckRequest& request,
                               const CheckResponse& expected_response) {
    // Check should not be called with cached entry
    EXPECT_CALL(*mock_transport_, Check(_, _, _)).Times(0);

    CheckResponse cached_response;
    Status cached_done_status = Status::UNKNOWN;
    client_->Check(request, &cached_response,
                   [& ret_status = cached_done_status](Status status) {
                     ret_status = status;
                   });
    // on_check_done is called inplace with a cached entry.
    EXPECT_OK(cached_done_status);
    EXPECT_TRUE(MessageDifferencer::Equals(expected_response, cached_response));

    // Verifies the expectation before client_ Destructor.  Its Destructor will
    // flush the entry out and call Transport->Check
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  }

  CheckRequest check_request1_;
  CheckResponse pass_check_response1_;
  CheckResponse error_check_response1_;
  ReportRequest report_request1_;

  std::unique_ptr<ServiceControlClient> client_;
  // This doesn't own the object. The object ownership is passed to client_
  MockNonBlockingTransport* mock_transport_;
};

TEST_F(ServiceControlClientImplTest, TestNonCachedCheckWithStoredCallback) {
  // This request is high important, so it will not be cached.
  // client->Check() will call Transport::Check() right away.
  check_request1_.mutable_operation()->set_importance(Operation::HIGH);
  InternalTestNonCachedCheckWithStoredCallback(check_request1_, Status::OK,
                                               &pass_check_response1_);

  // A High important request will not check cache, but its response will be
  // cached. Its cached response can be used by other Low important requests.
  check_request1_.mutable_operation()->set_importance(Operation::LOW);
  InternalTestCachedCheck(check_request1_, pass_check_response1_);
}

TEST_F(ServiceControlClientImplTest,
       TestFailedNonCachedCheckWithStoredCallback) {
  // This request is high important, so it will not be cached.
  // client->Check() will call Transport::Check() right away.
  check_request1_.mutable_operation()->set_importance(Operation::HIGH);

  // For al failed Check calls, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestNonCachedCheckWithStoredCallback(
        check_request1_, Status(Code::PERMISSION_DENIED, ""),
        &pass_check_response1_);
  }
}

TEST_F(ServiceControlClientImplTest, TestNonCachedCheckUsingThread) {
  // This request is high important, so it will not be cached.
  // client->Check() will call Transport::Check() right away.
  check_request1_.mutable_operation()->set_importance(Operation::HIGH);
  InternalTestNonCachedCheckUsingThread(check_request1_, Status::OK,
                                        &error_check_response1_);

  // A High important request will not check cache, but its response will be
  // cached. Its cached response can be used by other Low important requests.
  check_request1_.mutable_operation()->set_importance(Operation::LOW);
  InternalTestCachedCheck(check_request1_, error_check_response1_);
}

TEST_F(ServiceControlClientImplTest, TestFailedNonCachedCheckUsingThread) {
  // This request is high important, so it will not be cached.
  // client->Check() will call Transport::Check() right away.
  check_request1_.mutable_operation()->set_importance(Operation::HIGH);

  // For al failed Check calls, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestNonCachedCheckUsingThread(check_request1_,
                                          Status(Code::PERMISSION_DENIED, ""),
                                          &pass_check_response1_);
  }
}

TEST_F(ServiceControlClientImplTest, TestCachedReportWithStoredCallback) {
  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  // But Transport::Report() should not be called.
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status](Status status) { ret_status = status; });
  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  EXPECT_OK(done_status);

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithStoredCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(mock_transport_->on_done_ != NULL);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  // Call the on_check_done() to complete the data flow.
  mock_transport_->on_done_(Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportWithInplaceCallback) {
  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  // But Transport::Report() should not be called.
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status](Status status) { ret_status = status; });
  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  EXPECT_OK(done_status);

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithInplaceCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportUsingThread) {
  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  // But Transport::Report() should not be called.
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status](Status status) { ret_status = status; });
  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  EXPECT_OK(done_status);

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportUsingThread));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportWithStoredCallback) {
  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithStoredCallback));

  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status](Status status) { ret_status = status; });
  // on_report_done is not called yet. waiting for transport one_report_done.
  EXPECT_EQ(done_status, Status::UNKNOWN);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(mock_transport_->on_done_ != NULL);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  // Calls the on_check_done() to send status.
  mock_transport_->on_done_(Status(Code::PERMISSION_DENIED, ""));
  // on_report_done is called with right status.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportWithInplaceCallback) {
  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithInplaceCallback));

  // Set the report status to be used in the on_report_done
  mock_transport_->done_status_ = Status(Code::PERMISSION_DENIED, "");

  ReportResponse report_response;
  Status done_status = Status::UNKNOWN;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status](Status status) { ret_status = status; });

  // one_done should be called for now.
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, done_status);

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportUsingThread) {
  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportUsingThread));

  // Set the report status to be used in the on_report_done
  mock_transport_->done_status_ = Status(Code::PERMISSION_DENIED, "");

  std::promise<Status> status_promise;
  std::future<Status> status_future = status_promise.get_future();

  ReportResponse report_response;
  // This request is high important, so it will not be cached.
  // client->Report() will call Transport::Report() right away.
  report_request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  client_->Report(report_request1_, &report_response,
                  [& promise = status_promise](Status status) {
                    promise.set_value(status);
                  });

  // Since it is not cached, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  // on_report_done is called with right status.
  status_future.wait();
  EXPECT_ERROR_CODE(Code::PERMISSION_DENIED, status_future.get());
}

TEST_F(ServiceControlClientImplTest, TestFlushIntervalReportNeverFlush) {
  // Report never flush as its flush_interval_ms is -1.
  ReportAggregationOptions report_options(1 /* entries */,
                                          -1 /*flush_interval_ms*/);
  // Check flush interval is its expiration_ms.
  CheckAggregationOptions check_options(
      1 /*entries */, 500 /* refresh_interval_ms */, 1000 /* expiration_ms */);

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(
          kServiceName, check_options, report_options,
          std::shared_ptr<MetricKindMap>(new MetricKindMap)));
  ASSERT_EQ(client->GetNextFlushInterval(), 1000);
}

TEST_F(ServiceControlClientImplTest, TestFlushIntervalCheckNeverFlush) {
  ReportAggregationOptions report_options(1 /* entries */,
                                          500 /*flush_interval_ms*/);
  // If entries = 0, cache is disabled, GetNextFlushInterval() will be -1.
  CheckAggregationOptions check_options(
      0 /*entries */, 500 /* refresh_interval_ms */, 1000 /* expiration_ms */);

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(
          kServiceName, check_options, report_options,
          std::shared_ptr<MetricKindMap>(new MetricKindMap)));
  ASSERT_EQ(client->GetNextFlushInterval(), 500);
}

TEST_F(ServiceControlClientImplTest, TestFlushInterval) {
  ReportAggregationOptions report_options(1 /* entries */,
                                          800 /*flush_interval_ms*/);
  CheckAggregationOptions check_options(
      1 /*entries */, 500 /* refresh_interval_ms */, 1000 /* expiration_ms */);

  std::unique_ptr<ServiceControlClient> client =
      std::move(CreateServiceControlClient(
          kServiceName, check_options, report_options,
          std::shared_ptr<MetricKindMap>(new MetricKindMap)));
  ASSERT_EQ(client->GetNextFlushInterval(), 800);
}

}  // namespace service_control_client
}  // namespace google
