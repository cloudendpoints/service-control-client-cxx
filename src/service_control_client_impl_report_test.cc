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
  client_->Report(report_request1_, &report_response,
                  [&status_promise](Status status) {
                    StatusPromise moved_promise(std::move(status_promise));
                    moved_promise.set_value(status);
                  });

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
      QuotaAggregationOptions(1 /*entries */, 500 /* refresh_interval_ms */),
      ReportAggregationOptions(1 /* entries */, -1 /*flush_interval_ms*/));

  MockPeriodicTimer mock_timer;
  options.periodic_timer = mock_timer.GetFunc();
  EXPECT_CALL(mock_timer, StartTimer(_, _))
      .WillOnce(Invoke(&mock_timer, &MockPeriodicTimer::MyStartTimer));

  std::unique_ptr<ServiceControlClient> client =
      CreateServiceControlClient(kServiceName, kServiceConfigId, options);
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
