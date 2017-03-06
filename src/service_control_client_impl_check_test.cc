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

}  // namespace service_control_client
}  // namespace google
