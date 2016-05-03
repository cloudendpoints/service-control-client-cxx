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

// Utility functions used to generate signature for operations, metric values,
// and check requests.

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SIGNATURE_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SIGNATURE_H_

#include <string>
#include "google/api/servicecontrol/v1/metric_value.pb.h"
#include "google/api/servicecontrol/v1/operation.pb.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"

namespace google {
namespace service_control_client {

// Generates signature for an operation based on operation name and operation
// labels. Should be used only for report requests.
//
// Operations having the same signature can be aggregated or batched. Assuming
// all operations belong to the same service.
std::string GenerateReportOperationSignature(
    const ::google::api::servicecontrol::v1::Operation& operation);

// Generates signature for a metric value based on metric value labels, and
// currency code(For money value only). Should be used only for report requests.
//
// metric value with the same metric name and metric value signature can be
// merged.
std::string GenerateReportMetricValueSignature(
    const ::google::api::servicecontrol::v1::MetricValue& metric_value);

// Generates signature for a check request. Operation name, consumer id,
// operation labels, metric name, metric value labels, currency code(For money
// value only), quota properties, and request project settings are all included
// to generate the signature.
//
// Check request having the same signature can be aggregated. Assuming all
// requests belong to the same service.
std::string GenerateCheckRequestSignature(
    const ::google::api::servicecontrol::v1::CheckRequest& request);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SIGNATURE_H_
