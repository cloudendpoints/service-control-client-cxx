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

TEST_F(ServiceControlClientImplTest, TestFlushIntervalCheckNeverFlush) {
  // With periodic_timer, report flush interval is 500,
  // Check flush interval is -1 since its cache is disabled.
  // So the overall flush interval is 500
  ServiceControlClientOptions options(
      // If entries = 0, cache is disabled, GetNextFlushInterval() will be -1.
      CheckAggregationOptions(0 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  ASSERT_EQ(mock_timer.interval_ms_, 500);
}

TEST_F(ServiceControlClientImplTest, TestFlushInterval) {
  // With periodic_timer, report flush interval is 800, Check flush interval is
  // 1000, So the overall flush interval is 800
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
      ReportAggregationOptions(1 /* entries */, 800 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  ASSERT_EQ(mock_timer.interval_ms_, 800);
}

TEST_F(ServiceControlClientImplTest, TestFlushCalled) {
  // To test flush function is called properly with periodic_timer.
  ServiceControlClientOptions options(
      CheckAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */,
                              1000 /* expiration_ms */),
      QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.report_transport = mock_report_transport_.GetFunc();
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  client_ = CreateServiceControlClient(kServiceName, kServiceConfigId, options);
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
      QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
      ReportAggregationOptions(1 /* entries */, 500 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.report_transport = mock_report_transport_.GetFunc();
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  client_ = CreateServiceControlClient(kServiceName, kServiceConfigId, options);
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
