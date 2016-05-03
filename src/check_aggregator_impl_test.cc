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

#include "src/check_aggregator_impl.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <unistd.h>

using std::string;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

const char kServiceName[] = "library.googleapis.com";
const char kMetric[] = "library.googleapis.com/rpc/client/count";

const int kFlushIntervalMs = 100;
const int kExpirationMs = 200;

const char kRequest1[] = R"(
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

const char kSuccessResponse1[] = R"(
operation_id: "operation-1"
)";

const char kErrorResponse1[] = R"(
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

const char kRequest2[] = R"(
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

const char kSuccessResponse2[] = R"(
operation_id: "operation-2"
)";

const char kErrorResponse2[] = R"(
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

}  // namespace

class CheckAggregatorImplTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest1, &request1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse1, &pass_response1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kErrorResponse1, &error_response1_));

    ASSERT_TRUE(TextFormat::ParseFromString(kRequest2, &request2_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse2, &pass_response2_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kErrorResponse2, &error_response2_));

    CheckAggregationOptions options(1 /*entries*/, kFlushIntervalMs,
                                    kExpirationMs);

    aggregator_ = std::move(CreateCheckAggregator(
        kServiceName, options,
        std::shared_ptr<MetricKindMap>(new MetricKindMap)));
    ASSERT_TRUE((bool)(aggregator_));
    aggregator_->SetFlushCallback(std::bind(
        &CheckAggregatorImplTest::FlushCallback, this, std::placeholders::_1));
  }

  void FlushCallback(const CheckRequest& request) {
    flushed_.push_back(request);
  }

  void FlushCallbackCallingBackToAggregator(const CheckRequest& request) {
    flushed_.push_back(request);
    aggregator_->CacheResponse(request, pass_response1_);
  }

  CheckRequest request1_;
  CheckResponse pass_response1_;
  CheckResponse error_response1_;

  CheckRequest request2_;
  CheckResponse pass_response2_;
  CheckResponse error_response2_;

  std::unique_ptr<CheckAggregator> aggregator_;
  std::vector<CheckRequest> flushed_;
};

TEST_F(CheckAggregatorImplTest, TestNotMatchingServiceName) {
  *(request1_.mutable_service_name()) = "some-other-service-name";
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::INVALID_ARGUMENT,
                    aggregator_->Check(request1_, &response));
}

TEST_F(CheckAggregatorImplTest, TestNoOperation) {
  request1_.clear_operation();
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::INVALID_ARGUMENT,
                    aggregator_->Check(request1_, &response));
}

TEST_F(CheckAggregatorImplTest, TestHighValueOperationSuccess) {
  request1_.mutable_operation()->set_importance(Operation::HIGH);
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));
}

TEST_F(CheckAggregatorImplTest, TestDisableCache) {
  CheckAggregationOptions options(0 /*entries*/, 1000, 2000);
  aggregator_ = std::move(
      CreateCheckAggregator(kServiceName, options,
                            std::shared_ptr<MetricKindMap>(new MetricKindMap)));
  ASSERT_TRUE((bool)(aggregator_));
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));
}

TEST_F(CheckAggregatorImplTest, TestCachePassResponses) {
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));
}

TEST_F(CheckAggregatorImplTest, TestCacheErrorResponses) {
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_OK(aggregator_->CacheResponse(request1_, error_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, error_response1_));
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->FlushAll());
  // For error, only cache response, not aggregate request.
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(CheckAggregatorImplTest, TestCacheCapacity) {
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request2_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request2_, pass_response2_));
  EXPECT_OK(aggregator_->Check(request2_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response2_));
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 2);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[1], request2_));
}

TEST_F(CheckAggregatorImplTest, TestRefresh) {
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));

  // sleep 0.12 second.
  usleep(120000);

  // First one should be NOT_FOUND for refresh
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));
  // Second one use cached response.
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));

  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 1);

  // quota value in request has been aggregated 3 times
  // since Check() has been called 3 times after it is cached.
  request1_.mutable_operation()
      ->mutable_metric_value_sets(0)
      ->mutable_metric_values(0)
      ->set_int64_value(3000);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));
}

TEST_F(CheckAggregatorImplTest, TestCacheExpired) {
  CheckResponse response;
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));
  EXPECT_EQ(flushed_.size(), 0);

  // sleep 0.22 second to cause cache expired.
  usleep(220000);
  EXPECT_OK(aggregator_->Flush());

  // First one should be NOT_FOUND for refresh
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));

  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));
}

TEST_F(CheckAggregatorImplTest, TestFlushAllWithCallbackCallingCacheResposne) {
  aggregator_->SetFlushCallback(
      std::bind(&CheckAggregatorImplTest::FlushCallbackCallingBackToAggregator,
                this, std::placeholders::_1));

  CheckResponse response;

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_OK(aggregator_->FlushAll());
  // FlushAll() will call flush callback to flush out request1, then callback
  // will call CacheRequest().
  EXPECT_EQ(flushed_.size(), 1);
}

TEST_F(CheckAggregatorImplTest,
       TestCacheResponseWithCallbackCallingCacheResposne) {
  aggregator_->SetFlushCallback(
      std::bind(&CheckAggregatorImplTest::FlushCallbackCallingBackToAggregator,
                this, std::placeholders::_1));
  CheckResponse response;
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request2_, pass_response2_));
  // CacheResponse() will call flush callback to flush out request1 since cache
  // capacity is 1.
  EXPECT_EQ(flushed_.size(), 1);
}

TEST_F(CheckAggregatorImplTest, TestCheckWithCallbackCallingCacheResposne) {
  aggregator_->SetFlushCallback(
      std::bind(&CheckAggregatorImplTest::FlushCallbackCallingBackToAggregator,
                this, std::placeholders::_1));
  CheckResponse response;

  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Check(request1_, &response));

  usleep(220000);

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Check(request1_, &response));
  // Check() will evict request1 since it is expired.
  EXPECT_EQ(flushed_.size(), 1);
}

}  // namespace service_control_client
}  // namespace google
