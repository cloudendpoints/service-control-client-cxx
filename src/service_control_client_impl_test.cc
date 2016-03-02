#include "include/service_control_client.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <future>
#include <thread>
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

// A mocking class to mock NonBlockingTransport interface.
class MockNonBlockingTransport : public NonBlockingTransport {
 public:
  MOCK_METHOD3(Check, void(const CheckRequest&, CheckResponse*, DoneCallback));
  MOCK_METHOD3(Report,
               void(const ReportRequest&, ReportResponse*, DoneCallback));

  MockNonBlockingTransport()
      : check_response_(NULL), report_response_(NULL), on_done_(NULL) {}

  ~MockNonBlockingTransport() override {
    for (auto& callback_thread : callback_threads_) {
      callback_thread->join();
    }
  }

  // The done callback is stored in on_done_. It MUST be called later.
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
    callback_threads_.push_back(std::unique_ptr<std::thread>(new std::thread([
      on_done, done_status = done_status_, check_response = check_response_,
      response
    ]() {
      if (check_response) {
        *response = *check_response;
      }
      on_done(done_status);
    })));
  }

  // The done callback is stored in on_done_. It MUST be called later.
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
    callback_threads_.push_back(std::unique_ptr<std::thread>(new std::thread(
        [ on_done, done_status = done_status_ ]() { on_done(done_status); })));
  }

  // Saved check_request from mocked Transport::Check() call.
  CheckRequest check_request_;
  // If not NULL, the check response to send for mocked Transport::Check() call.
  CheckResponse* check_response_;
  // Saved report_request from mocked Transport::Report() call.
  ReportRequest report_request_;
  // If not NULL, the report response to send for mocked Transport::Report()
  // call.
  ReportResponse* report_response_;

  // saved on_done callback from either Transport::Check() or
  // Transport::Check().
  DoneCallback on_done_;
  // The status to send in on_done call back for Check() or Report().
  Status done_status_;
  // A vector to store thread objects used to call on_done callback.
  std::vector<std::unique_ptr<std::thread>> callback_threads_;
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
    EXPECT_CALL(*mock_transport_, Check(_, _, _))
        .WillOnce(Invoke(mock_transport_,
                         &MockNonBlockingTransport::CheckWithStoredCallback));

    // Set the check response.
    mock_transport_->check_response_ = transport_response;

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
    mock_transport_->on_done_(transport_status);
    // on_check_done is called with right status.
    EXPECT_EQ(done_status, transport_status);
    if (done_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
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
    EXPECT_CALL(*mock_transport_, Check(_, _, _))
        .WillOnce(Invoke(mock_transport_,
                         &MockNonBlockingTransport::CheckUsingThread));

    // Set the check status and response to be used in the on_check_done
    mock_transport_->done_status_ = transport_status;
    mock_transport_->check_response_ = transport_response;

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
    EXPECT_EQ(transport_status, status_future.get());
    if (transport_status.ok()) {
      EXPECT_TRUE(
          MessageDifferencer::Equals(*transport_response, check_response));
    }

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
  }

  // Tests a cached check request.
  // 1) Calls a Client::Check(), its request is in the cache.
  // 2) Client::on_check_done() is called right away.
  // 3) Transport::Check() is not called.
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

    // Verifies call expections and clear it before other test.
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
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

  std::unique_ptr<ServiceControlClient> client_;
  // This doesn't own the object. The object ownership is passed to client_
  MockNonBlockingTransport* mock_transport_;
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
  // For a cached request, it can be called repeatly.
  for (int i = 0; i < 10; i++) {
    InternalTestCachedCheck(check_request1_, pass_check_response1_);
  }

  // There is a cached check request in the cache. When client is destroyed,
  // it will call Transport Check.
  EXPECT_CALL(*mock_transport_, Check(_, _, _))
      .WillOnce(
          Invoke(mock_transport_, &MockNonBlockingTransport::CheckUsingThread));
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
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithStoredCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(mock_transport_->on_done_ != NULL);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         merged_report_request_));

  // Call the on_check_done() to complete the data flow.
  mock_transport_->on_done_(Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportWithInplaceCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2, it should be cached.
  // Transport::Report() should not be called.
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithInplaceCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         merged_report_request_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestCachedReportUsingThread) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2, it should be cached.
  // Transport::Report() should not be called.
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send a merged_request.
  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportUsingThread));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         merged_report_request_));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportWithStoredCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(*mock_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithStoredCallback));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  EXPECT_TRUE(mock_transport_->on_done_ != NULL);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  mock_transport_->on_done_(Status::OK);
  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithStoredCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(mock_transport_->on_done_ != NULL);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request2_));

  // Call the on_check_done() to complete the data flow.
  mock_transport_->on_done_(Status::OK);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportWithInplaceCallback) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(*mock_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithInplaceCallback));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportWithInplaceCallback));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestReplacedReportUsingThread) {
  // Calls Client::Report() with request1, it should be cached.
  // Calls Client::Report() with request2 with different labels,
  // It should be cached with a new key. Since cache size is 1, reqeust1
  // should be cleared./ Transport::Report() should be called for request1.
  // After Client::FlushAll() is called, Transport::Report() should be called
  // to send request2.
  EXPECT_CALL(*mock_transport_, Report(_, _, _)).Times(0);

  ReportResponse report_response;
  Status done_status1 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request1_, &report_response,
      [& ret_status = done_status1](Status status) { ret_status = status; });
  EXPECT_OK(done_status1);

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", report_request2_.mutable_operations(0));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportUsingThread));

  Status done_status2 = Status::UNKNOWN;
  // this report should be cached,  one_done() should be called right away
  client_->Report(
      report_request2_, &report_response,
      [& ret_status = done_status2](Status status) { ret_status = status; });
  EXPECT_OK(done_status2);

  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request1_));

  // Verifies that mock_transport_::Report() is NOT called.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));

  EXPECT_CALL(*mock_transport_, Report(_, _, _))
      .WillOnce(Invoke(mock_transport_,
                       &MockNonBlockingTransport::ReportUsingThread));
  // Only after FlushAll(), mock_transport_::Report() is called.
  EXPECT_OK(client_->FlushAll());
  EXPECT_TRUE(MessageDifferencer::Equals(mock_transport_->report_request_,
                                         report_request2_));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_transport_));
}

TEST_F(ServiceControlClientImplTest, TestNonCachedReportWithStoredCallback) {
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in the same thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
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
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called inside Transport::Report() with error
  // PERMISSION_DENIED. The Client::done_done() is called with the same error.
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
  // Calls Client::Report with a high important request, it will not be cached.
  // Transport::Report() should be called.
  // Transport::on_done() is called in a separate thread with PERMISSION_DENIED
  // The Client::done_done() is called with the same error.
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
  // Report flush interval is -1, Check flush interval is 1000
  // So the overall flush interval is 1000

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
  // Report flush interval is 500,
  // Check flush interval is -1 since its cache is disabled.
  // So the overall flush interval is 500

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
  // Report flush interval is 800, Check flush interval is 1000
  // So the overall flush interval is 800

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
