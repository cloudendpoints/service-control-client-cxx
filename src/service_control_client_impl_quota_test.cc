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

#include "src/service_control_client_impl_test.h"

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
operation_id: "a197c6f2-aecc-4a31-9744-b1d5aea4e4b4"
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
  }

  AllocateQuotaRequest quota_request1_;
  AllocateQuotaResponse pass_quota_response1_;
  AllocateQuotaResponse error_quota_response1_;

  MockCheckTransport mock_check_transport_;
  MockQuotaTransport mock_quota_transport_;
  MockReportTransport mock_report_transport_;
};

TEST_F(ServiceControlClientImplQuotaTest,
       TestNonCachedQuotaWithStoredCallback) {
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(0, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  int callbackCalledCount = 0;
  options.quota_transport = [this, &callbackCalledCount](
      const AllocateQuotaRequest& request, AllocateQuotaResponse* response,
      TransportDoneFunc on_done) {
    response = &pass_quota_response1_;

    callbackCalledCount++;
    on_done(Status::OK);
  };

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });
  ASSERT_TRUE(TextFormat::ParseFromString(kSuccessResponse1, &quota_response));

  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; });
    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse1, &quota_response));
  }
  EXPECT_EQ(done_status, Status::OK);

  EXPECT_EQ(callbackCalledCount, 11);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 11);

  EXPECT_EQ(stat.send_report_operations, 0);
}

TEST_F(ServiceControlClientImplQuotaTest, TestCachedQuotaWithStoredCallback) {
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(10, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  int callbackCalledCount = 0;
  options.quota_transport = [&callbackCalledCount](
      const AllocateQuotaRequest& request, AllocateQuotaResponse* response,
      TransportDoneFunc on_done) {
    callbackCalledCount++;
    on_done(Status::OK);
  };

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; });

  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; });
  }

  EXPECT_EQ(done_status, Status::OK);

  EXPECT_EQ(callbackCalledCount, 1);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);

  EXPECT_EQ(stat.send_report_operations, 0);
}

TEST_F(ServiceControlClientImplQuotaTest, TestNonCachedQuotaWithCallback) {
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(0, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  int callbackCalledCount = 0;

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; },
                 [&callbackCalledCount](const AllocateQuotaRequest& request,
                                        AllocateQuotaResponse* response,
                                        TransportDoneFunc on_done) {
                   callbackCalledCount++;
                   on_done(Status::OK);
                 });

  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; },
                   [&callbackCalledCount](const AllocateQuotaRequest& request,
                                          AllocateQuotaResponse* response,
                                          TransportDoneFunc on_done) {
                     callbackCalledCount++;
                     on_done(Status::OK);
                   });
  }

  EXPECT_EQ(done_status, Status::OK);

  EXPECT_EQ(callbackCalledCount, 11);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 11);

  EXPECT_EQ(stat.send_report_operations, 0);
}

TEST_F(ServiceControlClientImplQuotaTest, TestCachedQuotaWithCallback) {
  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(10, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  int callbackCalledCount = 0;

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  Status done_status = Status::UNKNOWN;
  AllocateQuotaResponse quota_response;

  client_->Quota(quota_request1_, &quota_response,
                 [&done_status](Status status) { done_status = status; },
                 [&callbackCalledCount](const AllocateQuotaRequest& request,
                                        AllocateQuotaResponse* response,
                                        TransportDoneFunc on_done) {
                   callbackCalledCount++;
                   on_done(Status::OK);
                 });

  for (int i = 0; i < 10; i++) {
    client_->Quota(quota_request1_, &quota_response,
                   [&done_status](Status status) { done_status = status; },
                   [&callbackCalledCount](const AllocateQuotaRequest& request,
                                          AllocateQuotaResponse* response,
                                          TransportDoneFunc on_done) {
                     callbackCalledCount++;
                     on_done(Status::OK);
                   });
  }

  EXPECT_EQ(done_status, Status::OK);

  EXPECT_EQ(callbackCalledCount, 1);

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);

  EXPECT_EQ(stat.send_report_operations, 0);
}

TEST_F(ServiceControlClientImplQuotaTest, TestNonCachedQuotaInSync) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(Invoke(&mock_quota_transport_,
                             &MockQuotaTransport::AllocateQuotaUsingThread));

  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(0, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  AllocateQuotaResponse quota_response;
  Status done_status = client_->Quota(quota_request1_, &quota_response);
  EXPECT_EQ(done_status, Status::OK);

  for (int i = 0; i < 10; i++) {
    done_status = client_->Quota(quota_request1_, &quota_response);
    EXPECT_EQ(done_status, Status::OK);
  }

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 11);

  EXPECT_EQ(stat.send_report_operations, 0);
}

TEST_F(ServiceControlClientImplQuotaTest, TestCachedQuotaInSync) {
  EXPECT_CALL(mock_quota_transport_, Quota(_, _, _))
      .WillRepeatedly(Invoke(&mock_quota_transport_,
                             &MockQuotaTransport::AllocateQuotaUsingThread));

  ServiceControlClientOptions options(CheckAggregationOptions(0, 500, 1000),
                                      QuotaAggregationOptions(10, 500),
                                      ReportAggregationOptions(0, 500));

  options.check_transport = mock_check_transport_.GetFunc();
  options.quota_transport = mock_quota_transport_.GetFunc();
  options.report_transport = mock_report_transport_.GetFunc();

  std::unique_ptr<ServiceControlClient> client_ =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  AllocateQuotaResponse quota_response;
  Status done_status = client_->Quota(quota_request1_, &quota_response);
  EXPECT_EQ(done_status, Status::OK);

  for (int i = 0; i < 10; i++) {
    done_status = client_->Quota(quota_request1_, &quota_response);
    EXPECT_EQ(done_status, Status::OK);
  }

  Statistics stat;
  Status stat_status = client_->GetStatistics(&stat);

  EXPECT_EQ(stat_status, Status::OK);
  EXPECT_EQ(stat.total_called_quotas, 11);
  EXPECT_EQ(stat.send_quotas_by_flush, 0);
  EXPECT_EQ(stat.send_quotas_in_flight, 1);

  EXPECT_EQ(stat.send_report_operations, 0);
}

}  // namespace service_control_client
}  // namespace google
