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
#include "src/money_utils.h"
#include "src/signature.h"
#include "utils/distribution_helper.h"
#include "utils/stl_util.h"

#include "google/protobuf/stubs/logging.h"

using std::string;
using ::google::protobuf::Timestamp;
using google::api::MetricDescriptor;
using google::api::servicecontrol::v1::MetricValue;
using google::api::servicecontrol::v1::MetricValueSet;
using google::api::servicecontrol::v1::Operation;

namespace google {
namespace service_control_client {

namespace {

// Returns whether timestamp a is before b or not.
bool TimestampBefore(const Timestamp& a, const Timestamp& b) {
  return a.seconds() < b.seconds() ||
         (a.seconds() == b.seconds() && a.nanos() < b.nanos());
}

// Merges two metric values, with metric kind being Cumulative or
// Gauge.
//
// New value will override old value, based on the end time.
void MergeCumulativeOrGaugeMetricValue(const MetricValue& from,
                                       MetricValue* to) {
  if (TimestampBefore(from.end_time(), to->end_time())) return;

  *to = from;
}

// Merges two metric values, with metric kind being Delta.
//
// Time [from_start, from_end] and [to_start, to_end] will be merged to time
// [min(from_start, to_start), max(from_end, to_end)]. It is OK to have gap or
// overlap between the two time spans.
//
// For INT64/DOUBLE/MONEY/DISTRIBUTION, values will be added together,
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
    case MetricValue::kDoubleValue:
      to->set_double_value(to->double_value() + from.double_value());
      break;
    case MetricValue::kMoneyValue: {
      // Since the currency code is in included in the metric value signature,
      // the currency codes in from and to should be identical when they
      // reach here. We are being defensive here to double check.
      if (from.money_value().currency_code() ==
          to->money_value().currency_code()) {
        *to->mutable_money_value() =
            SaturatedAddMoney(from.money_value(), to->money_value());
      } else {
        GOOGLE_LOG(ERROR)
            << "Different currency code in MergeDeltaMetricValue. This "
               "indicates a bug in metric value signature logic.";
      }
    } break;
    case MetricValue::kDistributionValue:
      DistributionHelper::Merge(from.distribution_value(),
                                to->mutable_distribution_value());
      break;
    default:
      GOOGLE_LOG(WARNING) << "Unknown metric kind for: " << to->DebugString();
      break;
  }
}

// Merges one metric value into another.
void MergeMetricValue(MetricDescriptor::MetricKind metric_kind,
                      const MetricValue& from, MetricValue* to) {
  if (metric_kind == MetricDescriptor::DELTA) {
    MergeDeltaMetricValue(from, to);
  } else {
    MergeCumulativeOrGaugeMetricValue(from, to);
  }
}

}  //  namespace

OperationAggregator::OperationAggregator(
    const Operation& operation,
    const std::unordered_map<string, MetricDescriptor::MetricKind>*
        metric_kinds)
    : operation_(operation), metric_kinds_(metric_kinds) {
  MergeMetricValueSets(operation);

  // Clear the metric value sets in operation_.
  operation_.clear_metric_value_sets();
}

void OperationAggregator::MergeOperation(const Operation& operation) {
  if (operation.has_start_time()) {
    if (!operation_.has_start_time() ||
        TimestampBefore(operation.start_time(), operation_.start_time())) {
      *(operation_.mutable_start_time()) = operation.start_time();
    }
  }

  if (operation.has_end_time()) {
    if (!operation_.has_end_time() ||
        TimestampBefore(operation_.end_time(), operation.end_time())) {
      *(operation_.mutable_end_time()) = operation.end_time();
    }
  }

  MergeMetricValueSets(operation);
  MergeLogEntries(operation);
}

Operation OperationAggregator::ToOperationProto() const {
  Operation op(operation_);

  for (const auto& metric_value_set : metric_value_sets_) {
    MetricValueSet* set = op.add_metric_value_sets();
    set->set_metric_name(metric_value_set.first);

    for (const auto& metric_value : metric_value_set.second) {
      *(set->add_metric_values()) = metric_value.second;
    }
  }

  return op;
}

void OperationAggregator::MergeLogEntries(const Operation& operation) {
  for (const auto& entry : operation.log_entries()) {
    *(operation_.add_log_entries()) = entry;
  }
}

void OperationAggregator::MergeMetricValueSets(const Operation& operation) {
  for (const auto& metric_value_set : operation.metric_value_sets()) {
    // Intentionally use the side effect of [] to add missing keys.
    std::unordered_map<string, MetricValue>& metric_values =
        metric_value_sets_[metric_value_set.metric_name()];

    MetricDescriptor::MetricKind metric_kind = MetricDescriptor::DELTA;
    if (metric_kinds_) {
      metric_kind =
          FindWithDefault(*metric_kinds_, metric_value_set.metric_name(),
                          MetricDescriptor::DELTA);
    }
    for (const auto& metric_value : metric_value_set.metric_values()) {
      string signature = GenerateReportMetricValueSignature(metric_value);
      MetricValue* existing = FindOrNull(metric_values, signature);
      if (existing == nullptr) {
        metric_values.emplace(signature, metric_value);
      } else {
        MergeMetricValue(metric_kind, metric_value, existing);
      }
    }
  }
}

}  // namespace service_control_client
}  // namespace google
