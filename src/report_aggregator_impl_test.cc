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

#include "src/report_aggregator_impl.h"

#include "gmock/gmock.h"
#include "google/protobuf/stubs/logging.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/type/money.pb.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <unistd.h>

using std::string;
using ::google::api::MetricDescriptor;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

const char kServiceName[] = "library.googleapis.com";
const char kMetric[] = "library.googleapis.com/rpc/client/count";

const char kOperationId1[] = "operation-1";

const char kRequest1[] = R"(
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

const char kRequest2[] = R"(
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
const char kDeltaMerged12[] = R"(
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
}  // namespace

class ReportAggregatorImplTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest1, &request1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kRequest2, &request2_));
    ASSERT_TRUE(TextFormat::ParseFromString(kDeltaMerged12, &delta_merged12_));

    ReportAggregationOptions options(1 /*entries*/, 1000 /*flush_interval_ms*/);
    aggregator_ = std::move(CreateReportAggregator(
        kServiceName, options,
        std::shared_ptr<MetricKindMap>(new MetricKindMap)));
    ASSERT_TRUE((bool)(aggregator_));
    aggregator_->SetFlushCallback(std::bind(
        &ReportAggregatorImplTest::FlushCallback, this, std::placeholders::_1));
  }

  // Adds a label to the given operation.
  void AddLabel(const string& key, const string& value, Operation* operation) {
    (*operation->mutable_labels())[key] = value;
  }

  void FlushCallback(const ReportRequest& request) {
    flushed_.push_back(request);
  }

  void FlushCallbackCallingBackToAggregator(const ReportRequest& request) {
    flushed_.push_back(request);
    aggregator_->Flush();
  }

  ReportRequest request1_;
  ReportRequest request2_;
  ReportRequest delta_merged12_;

  ReportResponse response_;

  std::unique_ptr<ReportAggregator> aggregator_;
  std::vector<ReportRequest> flushed_;
};

TEST_F(ReportAggregatorImplTest, TestNotMatchingServiceName) {
  *(request1_.mutable_service_name()) = "some-other-service-name";
  EXPECT_ERROR_CODE(Code::INVALID_ARGUMENT, aggregator_->Report(request1_));
  // Nothing flush out
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(ReportAggregatorImplTest, TestNoOperation) {
  request1_.clear_operations();
  EXPECT_OK(aggregator_->Report(request1_));
  // Nothing flush out
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(ReportAggregatorImplTest, TestAddOperation1) {
  EXPECT_OK(aggregator_->Report(request1_));
  // Item cached, not flushed out
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));
}

TEST_F(ReportAggregatorImplTest, TestAddOperation12) {
  EXPECT_OK(aggregator_->Report(request1_));
  // Item cached, not flushed out
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->Report(request2_));
  // Item cached, not flushed out
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], delta_merged12_));
}

TEST_F(ReportAggregatorImplTest, TestCacheCapacity) {
  EXPECT_OK(aggregator_->Report(request1_));
  // Item cached, not flushed out
  EXPECT_EQ(flushed_.size(), 0);

  // request2_ has different operation signature. Constrained by capacity 1,
  // request1 will be evicted from cache.
  AddLabel("key1", "value1", request2_.mutable_operations(0));
  EXPECT_OK(aggregator_->Report(request2_));
  // cache size is 1, the request1 has been flushed out.
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 2);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[1], request2_));
}

TEST_F(ReportAggregatorImplTest, TestCacheExpiration) {
  EXPECT_OK(aggregator_->Report(request1_));
  // Item cached, nothing flushed out
  EXPECT_EQ(flushed_.size(), 0);

  EXPECT_OK(aggregator_->Flush());
  // Not expired yet, nothing flush out.
  EXPECT_EQ(flushed_.size(), 0);

  // sleep 1.2 second.
  usleep(1200000);
  EXPECT_OK(aggregator_->Flush());
  // Item should be expired now.
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));
}

TEST_F(ReportAggregatorImplTest, TestHighValueOperationSuccess) {
  request1_.mutable_operations(0)->set_importance(Operation::HIGH);
  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Report(request1_));

  // Nothing flush out.
  EXPECT_EQ(flushed_.size(), 0);
  // Nothing in the cache
  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(ReportAggregatorImplTest, TestDisableCache) {
  ReportAggregationOptions options(0 /*entries*/, 1000 /*flush_interval_ms*/);
  aggregator_ = std::move(CreateReportAggregator(
      kServiceName, options,
      std::shared_ptr<MetricKindMap>(new MetricKindMap)));
  ASSERT_TRUE((bool)(aggregator_));
  aggregator_->SetFlushCallback(std::bind(
      &ReportAggregatorImplTest::FlushCallback, this, std::placeholders::_1));

  EXPECT_ERROR_CODE(Code::NOT_FOUND, aggregator_->Report(request1_));
  // Nothing flush out.
  EXPECT_EQ(flushed_.size(), 0);
  // Nothing in the cache
  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 0);
}

TEST_F(ReportAggregatorImplTest, TestFlushAllWithCallbackCallingFlush) {
  aggregator_->SetFlushCallback(
      std::bind(&ReportAggregatorImplTest::FlushCallbackCallingBackToAggregator,
                this, std::placeholders::_1));

  EXPECT_OK(aggregator_->Report(request1_));
  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 1);
}

TEST_F(ReportAggregatorImplTest, TestReportWithCallbackCallingFlush) {
  aggregator_->SetFlushCallback(
      std::bind(&ReportAggregatorImplTest::FlushCallbackCallingBackToAggregator,
                this, std::placeholders::_1));

  EXPECT_OK(aggregator_->Report(request1_));
  AddLabel("key1", "value1", request2_.mutable_operations(0));
  EXPECT_OK(aggregator_->Report(request2_));
  // Report(request2_) will evict request1_ out since the cacahe capacity is 1.
  EXPECT_EQ(flushed_.size(), 1);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[0], request1_));

  EXPECT_OK(aggregator_->FlushAll());
  EXPECT_EQ(flushed_.size(), 2);
  EXPECT_TRUE(MessageDifferencer::Equals(flushed_[1], request2_));
}

}  // namespace service_control_client
}  // namespace google
