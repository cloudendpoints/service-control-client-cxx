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

#include "src/mock_transport.h"

namespace google {
namespace service_control_client {

namespace {

const char kServiceName[] = "library.googleapis.com";
const char kServiceConfigId[] = "2016-09-19r0";

const char kRequest1[] = R"(
service_name: "library.googleapis.com"
allocate_operation {
  operation_id: "operation-1"
  method_name: "methodname"
  consumer_id: "consumerid"
  quota_metrics {
    metric_name: "metric_first"
    metric_values {
      int64_value: 1
    }
  }
  quota_metrics {
    metric_name: "metric_second"
    metric_values {
      int64_value: 1
    }
  }
  quota_mode: NORMAL
}
service_config_id: "2016-09-19r0"
)";

const char kSuccessResponse1[] = R"(
operation_id: "operation-1"
quota_metrics {
  metric_name: "serviceruntime.googleapis.com/api/consumer/quota_used_count"
  metric_values {
    labels {
      key: "/quota_name"
      value: "metric_first"
    }
    int64_value: 1
  }
  metric_values {
    labels {
      key: "/quota_name"
      value: "metric_first"
    }
    int64_value: 1
  }
}
service_config_id: "2016-09-19r0"
)";

const char kErrorResponse1[] = R"(
operation_id: "operation-1"
allocate_errors {
  code: RESOURCE_EXHAUSTED
  subject: "user:integration_test_user"
}
)";

}  // namespace

class ServiceControlClientImplQuotaTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest1, &quota_request1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse1, &pass_quota_response1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kErrorResponse1, &error_quota_response1_));

    ServiceControlClientOptions options(
        CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                                1000 /* expiration_ms */),
        QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
        ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

    options.check_transport = mock_check_transport_.GetFunc();
    options.quota_transport = mock_quota_transport_.GetFunc();
    options.report_transport = mock_report_transport_.GetFunc();

    client_ =
        CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  }

  AllocateQuotaRequest quota_request1_;
  AllocateQuotaResponse pass_quota_response1_;
  AllocateQuotaResponse error_quota_response1_;

  MockCheckTransport mock_check_transport_;
  MockQuotaTransport mock_quota_transport_;
  MockReportTransport mock_report_transport_;

  std::unique_ptr<ServiceControlClient> client_;
};

// Error on different service name
TEST_F(ServiceControlClientImplQuotaTest, TestQuotaWithInvalidServiceName) {
  ServiceControlClientOptions options(CheckAggregationOptions(1, 500, 1000),
                                      QuotaAggregationOptions(1, 500),
                                      ReportAggregationOptions(1, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient("unknown", kServiceConfigId, options);

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client->Quota(quota_request1_, &quota_response,
                [&done_status](Status status) { done_status = status; });

  EXPECT_EQ(
      done_status,
      Status(
          Code::INVALID_ARGUMENT,
          "Invalid service name: library.googleapis.com Expecting: unknown"));

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 0);
}

// Error on default callback is not assigned
TEST_F(ServiceControlClientImplQuotaTest,
       TestQuotaWithNoDefaultQuotaTransport) {
  ServiceControlClientOptions options(CheckAggregationOptions(10, 500, 1000),
                                      QuotaAggregationOptions(10, 500),
                                      ReportAggregationOptions(10, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = nullptr;
  options.report_transport = mock_report_transport_.GetFunc();

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;
  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient("unknown", kServiceConfigId, options);

  client->Quota(quota_request1_, &quota_response,
                [&done_status](Status status) { done_status = status; });

  EXPECT_EQ(done_status, Status(Code::INVALID_ARGUMENT, "transport is NULL."));

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 0);
}

// Cached: false, Callback: stored
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithStoredCallback) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithStoredCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });

  EXPECT_EQ(done_status, Status::UNKNOWN);

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 1);

  // execute stored callback
  mock_quota_transport_.on_done_vector_[0](done_status.OK);
  EXPECT_EQ(done_status, Status::OK);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: stored
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithStoredCallbackMultipleReqeusts) {
  // Initialize client instance with cache disabled
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(0, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(
          Invoke(&mock_quota_transport_,
                 &MockQuotaTransport::AllocateQuotaWithStoredCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  // call Quota 10 times
  for (int i = 0; i < 10; i++) {
    client->Quota(quota_request1_, &quota_response,
                  [&done_status](Status status) { done_status = status; });
    EXPECT_EQ(done_status, Status::UNKNOWN);
  }

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 10);

  // execute stored callback
  for (auto callback : mock_quota_transport_.on_done_vector_) {
    done_status = Status::UNKNOWN;
    callback(done_status.OK);
    EXPECT_EQ(done_status, Status::OK);
  }

  Statistics stat;
  Status stat_status = client->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 10);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 10);
}

// Cached: true, Callback: stored
TEST_F(ServiceControlClientImplQuotaTest,
       TestCachedQuotaWithStoredCallbackMultipleRequests) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithStoredCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });
  EXPECT_EQ(done_status, Status::UNKNOWN);

  // call Quota 10 times
  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; });
    EXPECT_EQ(done_status, Status::OK);
  }

  // count stored callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 1);

  // execute stored callback
  mock_quota_transport_.on_done_vector_[0](done_status.OK);
  EXPECT_EQ(done_status, Status::OK);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithInPlaceCallbackMultipleReqeusts) {
  // Initialize client instance with cache disabled
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(0, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(
          Invoke(&mock_quota_transport_,
                 &MockQuotaTransport::AllocateQuotaWithInplaceCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  // call Quota 10 times
  for (int i = 0; i < 10; i++) {
    client->Quota(quota_request1_, &quota_response,
                  [&done_status](Status status) { done_status = status; });
    EXPECT_EQ(done_status, Status::OK);
  }

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 10);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 10);
}

// Cached: false, Callback: in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithInPlaceCallback) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithInplaceCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });

  EXPECT_EQ(done_status, Status::OK);

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestCachedQuotaWithInPlaceCallbackMultipleRequests) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithInplaceCallback));

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });

  EXPECT_EQ(done_status, Status::OK);

  // call Quota 10 times
  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; });
    EXPECT_EQ(done_status, Status::OK);
  }

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: local in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithLocalInPlaceCallback) {
  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  int callbackExecuteCount = 0;

  client_->Quota(
      quota_request1_, &quota_response,
      [&done_status](Status status) { done_status = status; },
      [this, &callbackExecuteCount](const AllocateQuotaRequest& request,
                                    AllocateQuotaResponse* response,
                                    TransportDoneFunc on_done) {
        callbackExecuteCount++;
        on_done(Status::OK);
      });

  EXPECT_EQ(done_status, Status::OK);

  EXPECT_EQ(callbackExecuteCount, 1);

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithInPlaceCallbackInSync) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithInplaceCallback));

  AllocateQuotaResponse quota_response;

  Status done_status = client_->Quota(quota_request1_, &quota_response);

  EXPECT_EQ(done_status, Status::OK);

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: true, Callback: in place
TEST_F(ServiceControlClientImplQuotaTest,
       TestCachedQuotaWithInPlaceCallbackInSyncMultipleRequests) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillOnce(Invoke(&mock_quota_transport_,
                       &MockQuotaTransport::AllocateQuotaWithInplaceCallback));

  AllocateQuotaResponse quota_response;

  for (int i = 0; i < 10; i++) {
    Status done_status = client_->Quota(quota_request1_, &quota_response);
    EXPECT_EQ(done_status, Status::OK);
  }

  // count store callback
  EXPECT_EQ(mock_quota_transport_.on_done_vector_.size(), 0);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 10);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: true, Callback: thread
TEST_F(ServiceControlClientImplQuotaTest,
       TestCachedQuotaInSyncThreadMultipleRequests) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(Invoke(&mock_quota_transport_,
                             &MockQuotaTransport::AllocateQuotaUsingThread));

  Status done_status;
  AllocateQuotaResponse quota_response;

  // Set the check status and response to be used in the on_quota_done
  mock_quota_transport_.done_status_ = done_status;
  mock_quota_transport_.quota_response_ = &quota_response;

  done_status = client_->Quota(quota_request1_, &quota_response);
  EXPECT_TRUE(MessageDifferencer::Equals(mock_quota_transport_.quota_request_,
                                         quota_request1_));
  EXPECT_EQ(done_status, Status::OK);

  for (int i = 0; i < 10; i++) {
    done_status = client_->Quota(quota_request1_, &quota_response);
    EXPECT_TRUE(MessageDifferencer::Equals(mock_quota_transport_.quota_request_,
                                           quota_request1_));
    EXPECT_EQ(done_status, Status::OK);
  }

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: false, Callback: thread
TEST_F(ServiceControlClientImplQuotaTest, TestNonCachedQuotaThread) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(Invoke(&mock_quota_transport_,
                             &MockQuotaTransport::AllocateQuotaUsingThread));

  Status done_status;
  AllocateQuotaResponse quota_response;

  // Set the check status and response to be used in the on_quota_done
  mock_quota_transport_.done_status_ = done_status;
  mock_quota_transport_.quota_response_ = &quota_response;

  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();

  client_->Quota(quota_request1_, &quota_response,
                 [&status_promise, &done_status](Status status) {
                   StatusPromise moved_promise(std::move(status_promise));
                   moved_promise.set_value(status);
                 });

  // since it is a cache miss, transport should be called.
  EXPECT_TRUE(MessageDifferencer::Equals(mock_quota_transport_.quota_request_,
                                         quota_request1_));

  status_future.wait();
  EXPECT_EQ(status_future.get(), Status::OK);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 1);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

// Cached: true, Callback: thread
TEST_F(ServiceControlClientImplQuotaTest,
       TestCachedQuotaThreadMultipleRequests) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(Invoke(&mock_quota_transport_,
                             &MockQuotaTransport::AllocateQuotaUsingThread));

  Status done_status;
  AllocateQuotaResponse quota_response;

  // Set the check status and response to be used in the on_quota_done
  mock_quota_transport_.done_status_ = done_status;
  mock_quota_transport_.quota_response_ = &quota_response;

  StatusPromise status_promise;
  StatusFuture status_future = status_promise.get_future();
  client_->Quota(quota_request1_, &quota_response,
                 [&status_promise, &done_status](Status status) {
                   StatusPromise moved_promise(std::move(status_promise));
                   moved_promise.set_value(status);
                 });
  EXPECT_TRUE(MessageDifferencer::Equals(mock_quota_transport_.quota_request_,
                                         quota_request1_));
  status_future.wait();
  EXPECT_EQ(status_future.get(), Status::OK);

  for (int i = 0; i < 10; i++) {
    StatusPromise status_promise;
    StatusFuture status_future = status_promise.get_future();
    client_->Quota(quota_request1_, &quota_response,
                   [&status_promise, &done_status](Status status) {
                     StatusPromise moved_promise(std::move(status_promise));
                     moved_promise.set_value(status);
                   });
    EXPECT_TRUE(MessageDifferencer::Equals(mock_quota_transport_.quota_request_,
                                           quota_request1_));
    status_future.wait();
    EXPECT_EQ(status_future.get(), Status::OK);
  }

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);
}

}  // namespace service_control_client
}  // namespace google
