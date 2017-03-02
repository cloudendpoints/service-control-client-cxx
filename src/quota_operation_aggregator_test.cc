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

#include "src/quota_operation_aggregator.h"

#include <unordered_map>

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"

#include <unistd.h>

using std::string;
using ::google::api::servicecontrol::v1::QuotaOperation;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

const char kOperation1[] = R"(
operation_id: "operationid"
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
)";

const char kOperation2[] = R"(
operation_id: "operationid"
method_name: "methodname"
consumer_id: "consumerid"
quota_metrics {
  metric_name: "metric_first"
  metric_values {
    int64_value: 2
  }
}
quota_metrics {
  metric_name: "metric_second"
  metric_values {
    int64_value: 3
  }
}
quota_mode: NORMAL
)";

const std::set<std::pair<std::string, int>> ExtractMetricSets(
    const ::google::api::servicecontrol::v1::QuotaOperation& operation) {
  std::set<std::pair<std::string, int>> sets;

  for (auto quota_metric : operation.quota_metrics()) {
    sets.insert(std::make_pair(quota_metric.metric_name(),
                               quota_metric.metric_values(0).int64_value()));
  }

  return sets;
}

}  // namespace

class QuotaOperationAggregatorImplTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(TextFormat::ParseFromString(kOperation1, &operation1_));
    ASSERT_TRUE(TextFormat::ParseFromString(kOperation2, &operation2_));
  }

  ::google::api::servicecontrol::v1::QuotaOperation operation1_;
  ::google::api::servicecontrol::v1::QuotaOperation operation2_;
};

TEST_F(QuotaOperationAggregatorImplTest, TestInitialization) {
  QuotaOperationAggregator aggregator(operation1_);

  QuotaOperation operation = aggregator.ToOperationProto();

  std::set<std::pair<std::string, int>> quota_metrics =
      ExtractMetricSets(operation);
  std::set<std::pair<std::string, int>> expected_costs = {{"metric_first", 1},
                                                          {"metric_second", 1}};
  ASSERT_EQ(quota_metrics, expected_costs);
}

TEST_F(QuotaOperationAggregatorImplTest, TestMergeOperation) {
  QuotaOperationAggregator aggregator(operation1_);

  aggregator.MergeOperation(operation2_);

  QuotaOperation operation = aggregator.ToOperationProto();
  std::set<std::pair<std::string, int>> quota_metrics =
      ExtractMetricSets(operation);
  std::set<std::pair<std::string, int>> expected_costs = {{"metric_first", 3},
                                                          {"metric_second", 4}};
  ASSERT_EQ(quota_metrics, expected_costs);
}

}  // namespace service_control_client
}  // namespace google
