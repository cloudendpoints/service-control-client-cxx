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

#ifndef SRC_QUOTA_OPERATION_AGGREGATOR_H_
#define SRC_QUOTA_OPERATION_AGGREGATOR_H_

#include "google/api/metric.pb.h"
#include "google/api/servicecontrol/v1/quota_controller.pb.h"
#include "utils/google_macros.h"

namespace google {
namespace service_control_client {

class QuotaOperationAggregator {
 public:
  // Constructor. Does not take ownership of metric_kinds, which must outlive
  // this instance.
  QuotaOperationAggregator(
      const ::google::api::servicecontrol::v1::QuotaOperation& operation,
      const std::unordered_map<std::string,
                               ::google::api::MetricDescriptor::MetricKind>*
          metric_kinds);
  virtual ~QuotaOperationAggregator(){};

  // QuotaOperationsAggregator is neither copyable nor movable.
  QuotaOperationAggregator(const QuotaOperationAggregator&) = delete;
  QuotaOperationAggregator& operator=(const QuotaOperationAggregator&) = delete;

 public:
  // Merges the given operation with this operation, assuming the given
  // operation has the same operation signature.
  bool MergeOperation(
      const ::google::api::servicecontrol::v1::QuotaOperation& operation);

  // Transforms to Operation proto message.
  ::google::api::servicecontrol::v1::QuotaOperation ToOperationProto() const;

  // Check if the operation is too big.
  bool TooBig() const;

 private:
  // Merges the metric value sets in the given operation into this operation.
  bool MergeMetricValueSets(
      const ::google::api::servicecontrol::v1::QuotaOperation& operation);

  // Used to store everything but metric value sets.
  ::google::api::servicecontrol::v1::QuotaOperation operation_;

  // Aggregated metric values in the operation.
  // Key is metric_name.
  // Value is a map of metric value signature to aggregated metric value.
  std::unordered_map<
      std::string,
      std::unordered_map<std::string,
                         ::google::api::servicecontrol::v1::MetricValue>>
      metric_value_sets_;

  // Metric kinds. Key is the metric name and value is the metric kind.
  // Defaults to DELTA if not specified.
  const std::unordered_map<
      std::string, ::google::api::MetricDescriptor::MetricKind>* metric_kinds_;
};

}  // namespace service_control_client
}  // namespace google
#endif  // SRC_QUOTA_OPERATION_AGGREGATOR_H_
