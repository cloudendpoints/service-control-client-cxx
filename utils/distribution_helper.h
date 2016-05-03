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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_DISTRIBUTION_HELPER_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_DISTRIBUTION_HELPER_H_

#include "google/api/servicecontrol/v1/distribution.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {

// A helper class for handling Distribution proto message.
// Thread safe.
class DistributionHelper final {
 public:
  // Inits the distribution with exponential buckets.
  static ::google::protobuf::util::Status InitExponential(
      int num_finite_buckets, double growth_factor, double scale,
      ::google::api::servicecontrol::v1::Distribution* distribution);

  // Inits the distribution with linear buckets.
  static ::google::protobuf::util::Status InitLinear(
      int num_finite_buckets, double width, double offset,
      ::google::api::servicecontrol::v1::Distribution* distribution);

  // Inits the distribution with explicit buckets. Note that bounds must be
  // sorted in ascending order and contain no duplicates.
  static ::google::protobuf::util::Status InitExplicit(
      const std::vector<double>& bounds,
      ::google::api::servicecontrol::v1::Distribution* distribution);

  // Adds one more sample to the given distribution.
  static ::google::protobuf::util::Status AddSample(
      double value,
      ::google::api::servicecontrol::v1::Distribution* distribution);

  // Merges the "from" distribution to "to" distribution.
  // No change if the bucket options does not match.
  static ::google::protobuf::util::Status Merge(
      const ::google::api::servicecontrol::v1::Distribution& from,
      ::google::api::servicecontrol::v1::Distribution* to);
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_DISTRIBUTION_HELPER_H_
