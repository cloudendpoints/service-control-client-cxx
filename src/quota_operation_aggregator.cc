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

#include <iostream>

#include "src/money_utils.h"
#include "src/signature.h"
#include "utils/distribution_helper.h"
#include "utils/stl_util.h"

using std::string;
using ::google::protobuf::Timestamp;
using google::api::MetricDescriptor;
using google::api::servicecontrol::v1::MetricValue;
using google::api::servicecontrol::v1::MetricValueSet;
using google::api::servicecontrol::v1::QuotaOperation;

namespace google {
namespace service_control_client {

namespace {

// Returns whether timestamp a is before b or not.
bool TimestampBefore(const Timestamp& a, const Timestamp& b) {
  return a.seconds() < b.seconds() ||
         (a.seconds() == b.seconds() && a.nanos() < b.nanos());
}

// Merges two metric values, with metric kind being Delta.
//
// For INT64, values will be added together,
// except no change when the bucket options does not match.
void MergeDeltaMetricValue(const MetricValue& from, MetricValue* to) {
  if (to->value_case() != from.value_case()) {
    GOOGLE_LOG(WARNING) << "Metric values are not compatible: "
                        << from.DebugString() << ", " << to->DebugString();
    return;
  }

  if (from.has_start_time()) {
    if (!to->has_start_time() ||
        TimestampBefore(from.start_time(), to->start_time())) {
      *(to->mutable_start_time()) = from.start_time();
    }
  }

  if (from.has_end_time()) {
    if (!to->has_end_time() ||
        TimestampBefore(to->end_time(), from.end_time())) {
      *(to->mutable_end_time()) = from.end_time();
    }
  }

  switch (to->value_case()) {
    case MetricValue::kInt64Value:
      to->set_int64_value(to->int64_value() + from.int64_value());
      break;
    default:
      GOOGLE_LOG(WARNING) << "Unknown metric kind for: " << to->DebugString();
      break;
  }
}

}  // namespace anonymous

QuotaOperationAggregator::QuotaOperationAggregator(
    const ::google::api::servicecontrol::v1::QuotaOperation& operation)
    : operation_(operation) {
  MergeOperation(operation);
}

void QuotaOperationAggregator::MergeOperation(const QuotaOperation& operation) {
  for (const auto& metric_value_set : operation.quota_metrics()) {
    GOOGLE_DCHECK(metric_value_set.metric_values_size() == 1);

    auto found = metric_value_sets_.find(metric_value_set.metric_name());
    if (found == metric_value_sets_.end()) {
      metric_value_sets_[metric_value_set.metric_name()] =
          metric_value_set.metric_values(0);
    } else {
      MergeDeltaMetricValue(metric_value_set.metric_values(0), &found->second);
    }
  }
}

QuotaOperation QuotaOperationAggregator::ToOperationProto() const {
  QuotaOperation op(operation_);
  op.clear_quota_metrics();

  for (auto metric_values : metric_value_sets_) {
    MetricValueSet* set = op.add_quota_metrics();
    set->set_metric_name(metric_values.first);

    *(set->add_metric_values()) = metric_values.second;
  }

  return op;
}

}  // namespace service_control_client
}  // namespace google
