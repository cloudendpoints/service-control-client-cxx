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

#include "src/operation_aggregator.h"

#include "gmock/gmock.h"
#include "google/protobuf/stubs/logging.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/type/money.pb.h"
#include "gtest/gtest.h"

using std::string;
using ::google::api::MetricDescriptor;
using ::google::api::servicecontrol::v1::Distribution;
using ::google::api::servicecontrol::v1::MetricValue;
using ::google::api::servicecontrol::v1::Operation;
using ::google::type::Money;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

namespace google {
namespace service_control_client {
namespace {

const char kMetric[] = "library.googleapis.com/rpc/client/count";

const char kUSD[] = "USD";
const char kCAD[] = "CAD";

const char kOperation1[] = R"(
operation_id: "some-operation-id"
operation_name: "google.example.library.v1.LibraryService.CreateShelf"
consumer_id: "project:some-consumer"
labels {
  key: "library.googleapis.com/resource_type"
  value: "book"
}
labels {
  key: "library.googleapis.com/resource_id"
  value: "projects/123/books/456"
}
labels {
  key: "library.googleapis.com/region"
  value: "us-central1"
}
labels {
  key: "library.googleapis.com/zone"
  value: "us-central1-a"
}
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
    labels {
      key: "method"
      value: "list"
    }
    start_time {
      seconds: 100
    }
    end_time {
      seconds: 300
    }
    int64_value: 1000
  }
}
)";

// operation2 is after operation1.
const char kOperation2[] = R"(
operation_id: "some-operation-id"
operation_name: "google.example.library.v1.LibraryService.CreateShelf"
consumer_id: "project:some-consumer"
labels {
  key: "library.googleapis.com/resource_type"
  value: "book"
}
labels {
  key: "library.googleapis.com/resource_id"
  value: "projects/123/books/456"
}
labels {
  key: "library.googleapis.com/region"
  value: "us-central1"
}
labels {
  key: "library.googleapis.com/zone"
  value: "us-central1-a"
}
start_time {
  seconds: 2000
  nanos: 2000
}
end_time {
  seconds: 4000
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
    labels {
      key: "method"
      value: "list"
    }
    start_time {
      seconds: 200
    }
    end_time {
      seconds: 400
    }
    int64_value: 2000
  }
}
)";

// Merge operation 1 into operation 2, assuming they have cumulative metrics.
const char kCumulativeMerged12[] = R"(
operation_id: "some-operation-id"
operation_name: "google.example.library.v1.LibraryService.CreateShelf"
consumer_id: "project:some-consumer"
start_time {
  seconds: 1000
  nanos: 2000
}
end_time {
  seconds: 4000
  nanos: 4000
}
labels {
  key: "library.googleapis.com/region"
  value: "us-central1"
}
labels {
  key: "library.googleapis.com/resource_id"
  value: "projects/123/books/456"
}
labels {
  key: "library.googleapis.com/resource_type"
  value: "book"
}
labels {
  key: "library.googleapis.com/zone"
  value: "us-central1-a"
}
metric_value_sets {
  metric_name: "library.googleapis.com/rpc/client/count"
  metric_values {
    labels {
      key: "method"
      value: "list"
    }
    start_time {
      seconds: 200
    }
    end_time {
      seconds: 400
    }
    int64_value: 2000
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
)";

// Merge operation 1 into operation 2, assuming they have delta metrics.
const char kDeltaMerged12[] = R"(
operation_id: "some-operation-id"
operation_name: "google.example.library.v1.LibraryService.CreateShelf"
consumer_id: "project:some-consumer"
start_time {
  seconds: 1000
  nanos: 2000
}
end_time {
  seconds: 4000
  nanos: 4000
}
labels {
  key: "library.googleapis.com/region"
  value: "us-central1"
}
labels {
  key: "library.googleapis.com/resource_id"
  value: "projects/123/books/456"
}
labels {
  key: "library.googleapis.com/resource_type"
  value: "book"
}
labels {
  key: "library.googleapis.com/zone"
  value: "us-central1-a"
}
metric_value_sets {
  metric_name: "library.googleapis.com/rpc/client/count"
  metric_values {
    labels {
      key: "method"
      value: "list"
    }
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
)";

// Distribution for [-1, 1, 3, 5]
const char kDistribution[] = R"(
count: 4
mean: 2
sum_of_squared_deviation: 20
minimum: -1
maximum: 5
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 1
}
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
)";

// Distribution for [-1, 1, 3, 5] and [-1, 1, 3, 5]
const char kSumDistribution[] = R"(
count: 8
mean: 2
sum_of_squared_deviation: 40
minimum: -1
maximum: 5
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 1
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

class OperationAggregatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(TextFormat::ParseFromString(kOperation1, &operation1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kOperation2, &operation2_));

    ASSERT_TRUE(TextFormat::ParseFromString(kCumulativeMerged12,
                                            &cumulateive_merged12_));

    ASSERT_TRUE(TextFormat::ParseFromString(kDeltaMerged12, &delta_merged12_));
  }

  // Set the first metric value of the first metric value set to be double.
  void SetDobuleValue(double value, Operation* operation) {
    GOOGLE_CHECK(operation->metric_value_sets_size() > 0 &&
                 operation->metric_value_sets(0).metric_values_size() > 0);
    operation->mutable_metric_value_sets(0)
        ->mutable_metric_values(0)
        ->set_double_value(value);
  }

  // Set the first metric value of the first metric value set to be double.
  void SetMoneyValue(const Money& value, Operation* operation) {
    GOOGLE_CHECK(operation->metric_value_sets_size() > 0 &&
                 operation->metric_value_sets(0).metric_values_size() > 0);
    *(operation->mutable_metric_value_sets(0)
          ->mutable_metric_values(0)
          ->mutable_money_value()) = value;
  }

  // Set the first metric value of the first metric value set to be double.
  void SetDistributionValue(const Distribution& value, Operation* operation) {
    GOOGLE_CHECK(operation->metric_value_sets_size() > 0 &&
                 operation->metric_value_sets(0).metric_values_size() > 0);
    *(operation->mutable_metric_value_sets(0)
          ->mutable_metric_values(0)
          ->mutable_distribution_value()) = value;
  }

  Money CreateMoney(const string& currency_code, int value) {
    Money money;
    money.set_currency_code(currency_code);
    money.set_units(value);
    return money;
  }

  Operation operation1_;
  Operation operation2_;

  Operation cumulateive_merged12_;

  Operation delta_merged12_;

  MetricValue metric_value_;

  const std::unordered_map<string, MetricDescriptor::MetricKind>
      cumulative_metric_kind_ = {{kMetric, MetricDescriptor::CUMULATIVE}};

  const std::unordered_map<string, MetricDescriptor::MetricKind>
      delta_metric_kind_ = {{kMetric, MetricDescriptor::DELTA}};
};

// Clears the start time of operation and metric values.
void ClearStartEndTime(Operation* operation) {
  operation->clear_start_time();
  operation->clear_end_time();
  MetricValue* value1 =
      operation->mutable_metric_value_sets(0)->mutable_metric_values(0);
  value1->clear_start_time();
  value1->clear_end_time();
}

TEST_F(OperationAggregatorTest, Cumulative_MergeOperation1AndOperation2) {
  OperationAggregator iop(operation1_, &cumulative_metric_kind_);
  iop.MergeOperation(operation2_);
  EXPECT_TRUE(MessageDifferencer::Equals(iop.ToOperationProto(),
                                         cumulateive_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_MergeOperation1AndOperation2) {
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);
  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_MergeOperation2AndOperation1) {
  // Merge order does not matter.
  // log_entries is a repeated field, the order is different if added in
  // different order.
  operation1_.clear_log_entries();
  operation2_.clear_log_entries();
  delta_merged12_.clear_log_entries();
  OperationAggregator iop(operation2_, &delta_metric_kind_);
  iop.MergeOperation(operation1_);

  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest,
       DefaultMetricKind_MergeOperation1AndOperation2) {
  std::unordered_map<string, MetricDescriptor::MetricKind> empty_map;
  OperationAggregator iop(operation1_, &empty_map);
  iop.MergeOperation(operation2_);
  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_InconsistentMetricValue) {
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  MetricValue* value =
      operation2_.mutable_metric_value_sets(0)->mutable_metric_values(0);
  value->set_double_value(110.0);
  value->mutable_start_time()->set_seconds(100);
  value->mutable_start_time()->set_seconds(200);
  iop.MergeOperation(operation2_);

  // Because the the two operations have different metric value type, they
  // won't be merged, so iop should still be the same as operation1_.
  // However, log entries are still merged.
  *(delta_merged12_.mutable_metric_value_sets(0)) =
      operation1_.metric_value_sets(0);
  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_FirstOperationMissingStartEndTime) {
  ClearStartEndTime(&operation1_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);

  // The merged operation should have the same time as operation2_.
  *(delta_merged12_.mutable_start_time()) = operation2_.start_time();
  *(delta_merged12_.mutable_end_time()) = operation2_.end_time();
  const MetricValue& value2 = operation2_.metric_value_sets(0).metric_values(0);
  MetricValue* merged_value =
      delta_merged12_.mutable_metric_value_sets(0)->mutable_metric_values(0);
  *(merged_value->mutable_start_time()) = value2.start_time();
  *(merged_value->mutable_end_time()) = value2.end_time();
  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_SecondOperationMissingStartEndTime) {
  ClearStartEndTime(&operation2_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);

  // The merged operation should have the same time as operation1_.
  *(delta_merged12_.mutable_start_time()) = operation1_.start_time();
  *(delta_merged12_.mutable_end_time()) = operation1_.end_time();
  const MetricValue& value1 = operation1_.metric_value_sets(0).metric_values(0);
  MetricValue* merged_value =
      delta_merged12_.mutable_metric_value_sets(0)->mutable_metric_values(0);
  *(merged_value->mutable_start_time()) = value1.start_time();
  *(merged_value->mutable_end_time()) = value1.end_time();

  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, Delta_BothOperationsMissingStartEndTime) {
  ClearStartEndTime(&operation1_);
  ClearStartEndTime(&operation2_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);

  // The merged operation should have no start and end time.
  ClearStartEndTime(&delta_merged12_);

  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, DeltaMetricKind_DoubleValue) {
  SetDobuleValue(10, &operation1_);
  SetDobuleValue(20, &operation2_);
  SetDobuleValue(30, &delta_merged12_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);

  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, DeltaMetricKind_MoneyValue) {
  Money money1 = CreateMoney(kUSD, 10);
  Money money2 = CreateMoney(kUSD, 20);
  Money sum = CreateMoney(kUSD, 30);

  SetMoneyValue(money1, &operation1_);
  SetMoneyValue(money2, &operation2_);
  SetMoneyValue(sum, &delta_merged12_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);
  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

TEST_F(OperationAggregatorTest, DeltaMetricKind_DistributionValue) {
  Distribution distribution;
  Distribution sum;

  ASSERT_TRUE(TextFormat::ParseFromString(kDistribution, &distribution));
  ASSERT_TRUE(TextFormat::ParseFromString(kSumDistribution, &sum));

  SetDistributionValue(distribution, &operation1_);
  SetDistributionValue(distribution, &operation2_);
  SetDistributionValue(sum, &delta_merged12_);
  OperationAggregator iop(operation1_, &delta_metric_kind_);
  iop.MergeOperation(operation2_);

  EXPECT_TRUE(
      MessageDifferencer::Equals(iop.ToOperationProto(), delta_merged12_));
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
