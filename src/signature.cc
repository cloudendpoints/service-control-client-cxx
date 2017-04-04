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

#include "src/signature.h"
#include "utils/md5.h"

#include <set>
#include <map>

using std::string;
using google::api::servicecontrol::v1::CheckRequest;
using google::api::servicecontrol::v1::MetricValue;
using google::api::servicecontrol::v1::Operation;
using google::api::servicecontrol::v1::AllocateQuotaRequest;
using google::api::servicecontrol::v1::QuotaOperation;

namespace google {
namespace service_control_client {
namespace {
const char kDelimiter[] = "\0";
const int kDelimiterLength = 1;

// Updates the give hasher with the given labels.
void UpdateHashLabels(const ::google::protobuf::Map<string, string>& labels,
                      MD5* hasher) {
  std::map<string, string> ordered_labels(labels.begin(), labels.end());
  for (const auto& label : ordered_labels) {
    // Note we must use the Update(void const *data, int size) function here
    // for the delimiter instead of Update(StringPiece data), because
    // StringPiece would use strlen and gets zero length.
    hasher->Update(kDelimiter, kDelimiterLength);
    hasher->Update(label.first);
    hasher->Update(kDelimiter, kDelimiterLength);
    hasher->Update(label.second);
  }
}

// Updates the give hasher with the given metric value.
void UpdateHashMetricValue(const MetricValue& metric_value, MD5* hasher) {
  UpdateHashLabels(metric_value.labels(), hasher);
}
}  // namespace

string GenerateReportOperationSignature(const Operation& operation) {
  MD5 hasher;
  hasher.Update(operation.consumer_id());
  hasher.Update(kDelimiter, kDelimiterLength);
  hasher.Update(operation.operation_name());

  UpdateHashLabels(operation.labels(), &hasher);

  return hasher.Digest();
}

string GenerateReportMetricValueSignature(const MetricValue& metric_value) {
  MD5 hasher;

  UpdateHashMetricValue(metric_value, &hasher);
  return hasher.Digest();
}

string GenerateCheckRequestSignature(const CheckRequest& request) {
  MD5 hasher;

  const Operation& operation = request.operation();
  hasher.Update(operation.operation_name());

  hasher.Update(kDelimiter, kDelimiterLength);
  hasher.Update(operation.consumer_id());

  hasher.Update(kDelimiter, kDelimiterLength);
  UpdateHashLabels(operation.labels(), &hasher);

  // keep sorted order of metric_name
  std::map<std::string, const ::google::protobuf::RepeatedPtrField<
                            ::google::api::servicecontrol::v1::MetricValue>*>
      metric_value_set_map;
  for (const auto& metric_value_set : operation.metric_value_sets()) {
    metric_value_set_map[metric_value_set.metric_name()] =
        &metric_value_set.metric_values();
  }

  for(const auto& metric_value_set: metric_value_set_map) {
    hasher.Update(kDelimiter, kDelimiterLength);
    hasher.Update(metric_value_set.first);

    for(const auto& metric_value: *metric_value_set.second) {
      UpdateHashMetricValue(metric_value, &hasher);
    }
  }

  hasher.Update(kDelimiter, kDelimiterLength);

  return hasher.Digest();
}

string GenerateAllocateQuotaRequestSignature(
    const AllocateQuotaRequest& request) {
  MD5 hasher;
  const QuotaOperation& operation = request.allocate_operation();
  hasher.Update(operation.method_name());

  hasher.Update(kDelimiter, kDelimiterLength);
  hasher.Update(operation.consumer_id());

  // order of metric_name can be changed. need to be sorted.
  std::set<std::string> metric_names;
  for (const auto& metric_value_set : operation.quota_metrics()) {
    metric_names.insert(metric_value_set.metric_name());
  }

  for (const auto& metric_name : metric_names) {
    hasher.Update(kDelimiter, kDelimiterLength);
    hasher.Update(metric_name);
  }
  return hasher.Digest();
}

}  // namespace service_control_client
}  // namespace google
