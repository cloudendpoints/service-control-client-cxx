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

#include "src/signature.h"
#include "utils/md5.h"

#include "google/protobuf/text_format.h"
#include "google/type/money.pb.h"
#include "gtest/gtest.h"

using std::string;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::MetricValue;
using ::google::api::servicecontrol::v1::Operation;
using ::google::type::Money;
using ::google::protobuf::TextFormat;

namespace google {
namespace service_control_client {
namespace {

const char kRegionLabel[] = "cloud.googleapis.com/region";
const char kResourceTypeLabel[] = "cloud.googleapis.com/resource_type";

const char kCustomLabel[] = "/type";

const char kCheckRequest[] = R"(
service_name: "chemisttest.googleapis.com"
operation {
  operation_id: "some-id"
  operation_name: "some-operation-name"
  consumer_id: "project:proven-catcher-789"
  start_time {
    seconds: 1421429665
    nanos: 653927303
  }
  end_time {
    seconds: 1421429665
    nanos: 653933882
  }
  labels {
    key: "cloud.googleapis.com/resource_type"
    value: "some-resource_type"
  }
  labels {
    key: "cloud.googleapis.com/zone"
    value: "us-central1-a"
  }
  labels {
    key: "cloud.googleapis.com/resource_id"
    value: "some-resource_id"
  }
  metric_value_sets {
    metric_name: "chemisttest.googleapis.com/chemisttest/memory_hour_usage"
    metric_values {
      labels {
        key: "servicecontrol.googleapis.com/consumer_id"
        value: "some-consumer_id"
      }
      start_time {
        seconds: 1421429365
        nanos: 340407982
      }
      end_time {
        seconds: 1421429665
        nanos: 654013127
      }
      int64_value: 380
    }
  }
}
)";

class SignatureUtilTest : public ::testing::Test {
 protected:
  SignatureUtilTest() {
    operation_.set_operation_name("some-operation-name");
    operation_.set_consumer_id("project_id:some-project-id");
  }

  void AddOperationLabel(const string& key, const string& value,
                         Operation* operation) {
    (*operation->mutable_labels())[key] = value;

    metric_value_.set_int64_value(1000);
  }

  void AddMetricValueLabel(const string& key, const string& value,
                           MetricValue* metric_value) {
    (*metric_value->mutable_labels())[key] = value;
  }

  Operation operation_;

  MetricValue metric_value_;
};

TEST_F(SignatureUtilTest, OperationWithNoLabel) {
  EXPECT_EQ("d056b16b88b914b40cd5a82470bc02a5",
            MD5::DebugString(GenerateReportOperationSignature(operation_)));
}

TEST_F(SignatureUtilTest, OperationWithLabels) {
  AddOperationLabel(kRegionLabel, "us-central1", &operation_);
  AddOperationLabel(kResourceTypeLabel, "instance", &operation_);

  EXPECT_EQ("93bc5c613fc4eabb2a40042f7f73f671",
            MD5::DebugString(GenerateReportOperationSignature(operation_)));
}

TEST_F(SignatureUtilTest, MetricValueWithNoLabel) {
  EXPECT_EQ(
      "d41d8cd98f00b204e9800998ecf8427e",
      MD5::DebugString(GenerateReportMetricValueSignature(metric_value_)));
}

TEST_F(SignatureUtilTest, MetricValueWithLabels) {
  AddMetricValueLabel(kCustomLabel, "disk", &metric_value_);

  EXPECT_EQ(
      "3f6bc74c0a4be6b6eeaab1faac30a365",
      MD5::DebugString(GenerateReportMetricValueSignature(metric_value_)));
}

TEST_F(SignatureUtilTest, MetricValueHavingMoney) {
  Money* money = metric_value_.mutable_money_value();
  money->set_currency_code("USD");
  money->set_units(1000);
  EXPECT_EQ(
      "dbe2168cd8ad1eb33d8f9f6b9aea7f52",
      MD5::DebugString(GenerateReportMetricValueSignature(metric_value_)));
}

TEST_F(SignatureUtilTest, CheckRequest) {
  CheckRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kCheckRequest, &request));
  EXPECT_EQ("71287bd97890da01fd14d3f8daef3db9",
            MD5::DebugString(GenerateCheckRequestSignature(request)));
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
