// TODO: Insert description here. (generated by jaebong)

#include "src/quota_aggregator_impl.h"

#include <unordered_map>

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <unistd.h>

using std::string;
using ::google::api::servicecontrol::v1::QuotaOperation;
using ::google::api::servicecontrol::v1::AllocateQuotaRequest;
using ::google::api::servicecontrol::v1::AllocateQuotaResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

const char kServiceName[] = "library.googleapis.com";
const char kServiceConfigId[] = "2016-09-19r0";

const int kFlushIntervalMs = 100;
const int kExpirationMs = 200;

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

const char kRequest2[] = R"(
service_name: "library.googleapis.com"
allocate_operation {
  operation_id: "operation-2"
  method_name: "methodname2"
  consumer_id: "consumerid2"
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

const char kRequest3[] = R"(
service_name: "library.googleapis.com"
allocate_operation {
  operation_id: "operation-3"
  method_name: "methodname3"
  consumer_id: "consumerid3"
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

const char kSuccessResponse2[] = R"(
operation_id: "550e8400-e29b-41d4-a716-446655440000"
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
      value: "metric_second"
    }
    int64_value: 1
  }
}
service_config_id: "2017-02-08r5"
)";

const char kErrorResponse2[] = R"(
operation_id: "a197c6f2-aecc-4a31-9744-b1d5aea4e4b4"
allocate_errors {
  code: RESOURCE_EXHAUSTED
  subject: "user:integration_test_user"
}
)";

const char kEmptyResponse[] = R"(
)";

}  // namespace

class QuotaAggregatorImplTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest1, &request1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest2, &request2_));
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest3, &request3_));

    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse1, &pass_response1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kSuccessResponse2, &pass_response2_));

    ASSERT_TRUE(
        TextFormat::ParseFromString(kErrorResponse1, &error_response1_));
    ASSERT_TRUE(
        TextFormat::ParseFromString(kErrorResponse2, &error_response2_));

    ASSERT_TRUE(TextFormat::ParseFromString(kEmptyResponse, &empty_response_));

    QuotaAggregationOptions options(10, kFlushIntervalMs);

    aggregator_ =
        CreateAllocateQuotaAggregator(kServiceName, kServiceConfigId, options);
    ASSERT_TRUE((bool)(aggregator_));

    aggregator_->SetFlushCallback(std::bind(
        &QuotaAggregatorImplTest::FlushCallback, this, std::placeholders::_1));
  }

  void FlushCallback(const AllocateQuotaRequest& request) {
    flushed_.push_back(request);
  }

  void FlushCallbackCallingBackToAggregator(
      const AllocateQuotaRequest& request) {
    flushed_.push_back(request);
    aggregator_->CacheResponse(request, pass_response1_);
  }

  AllocateQuotaRequest request1_;
  AllocateQuotaRequest request2_;
  AllocateQuotaRequest request3_;

  AllocateQuotaResponse pass_response1_;
  AllocateQuotaResponse pass_response2_;

  AllocateQuotaResponse error_response1_;
  AllocateQuotaResponse error_response2_;

  AllocateQuotaResponse empty_response_;

  std::unique_ptr<QuotaAggregator> aggregator_;
  std::vector<AllocateQuotaRequest> flushed_;
};

TEST_F(QuotaAggregatorImplTest, TestFirstReqeustNotFound) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
}

TEST_F(QuotaAggregatorImplTest, TestSecondRequestFound) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
  EXPECT_OK(aggregator_->Quota(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, empty_response_));
}

TEST_F(QuotaAggregatorImplTest, TestFirstReqeustSucceeded) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Quota(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));
}

TEST_F(QuotaAggregatorImplTest, TestFirstReqeustQuotaExceed) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request1_, error_response1_));
  EXPECT_OK(aggregator_->Quota(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, error_response1_));
}

TEST_F(QuotaAggregatorImplTest, TestNotMatchingServiceName) {
  *(request1_.mutable_service_name()) = "some-other-service-name";
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::INVALID_ARGUMENT,
                    aggregator_->Quota(request1_, &response));
}

TEST_F(QuotaAggregatorImplTest, TestNoOperation) {
  request1_.clear_allocate_operation();
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::INVALID_ARGUMENT,
                    aggregator_->Quota(request1_, &response));
}

TEST_F(QuotaAggregatorImplTest, TestFlushAggregatedRecord) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Quota(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));

  EXPECT_EQ(flushed_.size(), 0);

  // simulate refresh timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(110));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 1);
}

TEST_F(QuotaAggregatorImplTest, TestAggregatedRecordNoRefresh) {
  AllocateQuotaResponse response;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response));
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  EXPECT_OK(aggregator_->Quota(request1_, &response));
  EXPECT_TRUE(MessageDifferencer::Equals(response, pass_response1_));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(QuotaAggregatorImplTest, TestCacheRefreshAllAggregated) {
  AllocateQuotaResponse response1;
  AllocateQuotaResponse response2;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response1));
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request2_, &response2));
  EXPECT_OK(aggregator_->CacheResponse(request2_, pass_response2_));

  EXPECT_OK(aggregator_->Quota(request1_, &response1));
  EXPECT_OK(aggregator_->Quota(request2_, &response2));
  EXPECT_TRUE(MessageDifferencer::Equals(response1, pass_response1_));
  EXPECT_TRUE(MessageDifferencer::Equals(response2, pass_response2_));

  EXPECT_EQ(flushed_.size(), 0);

  // expire request1
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 1);

  // expire request2
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 2);
}

TEST_F(QuotaAggregatorImplTest, TestCacheRefreshOneAggregated) {
  AllocateQuotaResponse response1;
  AllocateQuotaResponse response2;

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request1_, &response1));
  EXPECT_OK(aggregator_->CacheResponse(request1_, pass_response1_));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Quota(request2_, &response2));
  EXPECT_OK(aggregator_->CacheResponse(request2_, pass_response2_));
  EXPECT_OK(aggregator_->Quota(request1_, &response1));
  EXPECT_TRUE(MessageDifferencer::Equals(response1, pass_response1_));
  EXPECT_TRUE(MessageDifferencer::Equals(response2, empty_response_));

  EXPECT_EQ(flushed_.size(), 0);

  // expire request1
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 1);

  // expire request2
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  EXPECT_OK(aggregator_->Flush());
  EXPECT_EQ(flushed_.size(), 1);
}

}  // namespace service_control_client
}  // namespace google
