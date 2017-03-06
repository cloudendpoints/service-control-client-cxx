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

// Tests non cached check request. Mocked transport::Check() is storing
// on_done() callback and call it in a delayed fashion within the same thread.
// 1) Call a Client::Check(),  the request is not in the cache.
// 2) Transport::Check() is called. Mocked transport::Check() stores
//    the on_done callback.
// 3) Client::Check() returns.  Client::on_check_done() is not called yet.
// 4) Transport::on_done() is called in the same thread.
// 5) Client::on_check_done() is called.
void ServiceControlClientImplTest::InternalTestNonCachedCheckWithStoredCallback(
    const CheckRequest& request, Status transport_status,
    CheckResponse* transport_response) {
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckWithStoredCallback));

  // Set the check response.
  mock_check_transport_.check_response_ = transport_response;
  size_t saved_done_vector_size = mock_check_transport_.on_done_vector_.size();

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
void ServiceControlClientImplTest::
    InternalTestNonCachedCheckWithInplaceCallback(
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

void ServiceControlClientImplTest::
    InternalTestNonCachedBlockingCheckWithInplaceCallback(
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
void ServiceControlClientImplTest::InternalTestNonCachedCheckUsingThread(
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

void ServiceControlClientImplTest::
    InternalTestNonCachedBlockingCheckUsingThread(
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
void ServiceControlClientImplTest::
    InternalTestReplacedGoodCheckWithStoredCallback(
        const CheckRequest& request2, Status transport_status2,
        CheckResponse* transport_response2, const CheckRequest& request1,
        Status transport_status1, CheckResponse* transport_response1) {
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke(&mock_check_transport_,
                       &MockCheckTransport::CheckWithStoredCallback));

  // Set the check response.
  mock_check_transport_.check_response_ = transport_response2;
  size_t saved_done_vector_size = mock_check_transport_.on_done_vector_.size();

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
void ServiceControlClientImplTest::
    InternalTestReplacedGoodCheckWithInplaceCallback(
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

void ServiceControlClientImplTest::
    InternalTestReplacedBlockingCheckWithInplaceCallback(
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
void ServiceControlClientImplTest::InternalTestReplacedGoodCheckUsingThread(
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

void ServiceControlClientImplTest::InternalTestReplacedBlockingCheckUsingThread(
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
void ServiceControlClientImplTest::InternalTestCachedCheck(
    const CheckRequest& request, const CheckResponse& expected_response) {
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

void ServiceControlClientImplTest::InternalTestCachedBlockingCheck(
    const CheckRequest& request, const CheckResponse& expected_response) {
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
