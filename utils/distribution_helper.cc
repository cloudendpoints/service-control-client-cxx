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

#include "distribution_helper.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>

using ::google::api::servicecontrol::v1::Distribution;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {
namespace {

// Concatenate a string with an non string.
template <class T>
std::string StrCat(const char* str, T v) {
  std::ostringstream buffer;
  buffer.str(str);
  buffer << v;
  return buffer.str();
}

// Updates general statistics other than bucket count.
void UpdateGeneralStatictics(double value, Distribution* distribution) {
  if (distribution->count() == 0) {
    distribution->set_count(1);
    distribution->set_maximum(value);
    distribution->set_minimum(value);
    distribution->set_mean(value);
    distribution->set_sum_of_squared_deviation(0);
  } else {
    int64_t count = distribution->count();
    double mean = distribution->mean();
    double new_mean = (count * mean + value) / (count + 1);

    // The formula is optimized for minimum round off error (no large values in
    // intermediate results).
    distribution->set_sum_of_squared_deviation(
        distribution->sum_of_squared_deviation() +
        ((value - mean) * (value - new_mean)));

    distribution->set_count(count + 1);
    distribution->set_minimum(std::min(value, distribution->minimum()));
    distribution->set_maximum(std::max(value, distribution->maximum()));

    distribution->set_mean(new_mean);
  }
}

inline bool IsCloseEnough(double x, double y) {
  const double epsilon = 1e-5;
  return std::abs(x - y) <= epsilon * std::abs(x);
}

// Checks whether the bucket definitions in the two distributions are
// approximately equal. We use this instead of
// MessageDifferencer::ApproximatelyEquals mainly for performance reasons.
bool BucketsApproximatelyEqual(const Distribution& first,
                               const Distribution& second) {
  if (first.bucket_option_case() != second.bucket_option_case()) {
    return false;
  }
  switch (first.bucket_option_case()) {
    case Distribution::kLinearBuckets: {
      const auto& first_linear = first.linear_buckets();
      const auto& second_linear = second.linear_buckets();
      if (first_linear.num_finite_buckets() !=
          second_linear.num_finite_buckets()) {
        return false;
      }
      if (!IsCloseEnough(first_linear.width(), second_linear.width())) {
        return false;
      }
      if (!IsCloseEnough(first_linear.offset(), second_linear.offset())) {
        return false;
      }
      break;
    }
    case Distribution::kExponentialBuckets: {
      const auto& first_exponential = first.exponential_buckets();
      const auto& second_exponential = second.exponential_buckets();
      if (first_exponential.num_finite_buckets() !=
          second_exponential.num_finite_buckets()) {
        return false;
      }
      if (!IsCloseEnough(first_exponential.growth_factor(),
                         second_exponential.growth_factor())) {
        return false;
      }
      if (!IsCloseEnough(first_exponential.scale(),
                         second_exponential.scale())) {
        return false;
      }
      break;
    }
    case Distribution::kExplicitBuckets: {
      const auto& first_explicit = first.explicit_buckets();
      const auto& second_explicit = second.explicit_buckets();
      if (first_explicit.bounds_size() != second_explicit.bounds_size()) {
        return false;
      }
      for (int i = 0; i < first_explicit.bounds_size(); ++i) {
        if (!IsCloseEnough(first_explicit.bounds(i),
                           second_explicit.bounds(i))) {
          return false;
        }
      }
      break;
    }
    default:
      return false;
  }
  return true;
}

void UpdateExponentialBucketCount(double value, Distribution* distribution) {
  const auto& exponential = distribution->exponential_buckets();
  int bucket_index = 0;
  if (value >= exponential.scale()) {
    // Should be put into bucket bucket_index, starting from 0.
    bucket_index = 1 + static_cast<int>(log2(value / exponential.scale()) /
                                        log2(exponential.growth_factor()));
    if (bucket_index > exponential.num_finite_buckets() + 1) {
      bucket_index = exponential.num_finite_buckets() + 1;
    }
  }
  distribution->set_bucket_counts(
      bucket_index, distribution->bucket_counts(bucket_index) + 1);
}

void UpdateLinearBucketCount(double value, Distribution* distribution) {
  const auto& linear = distribution->linear_buckets();
  double upper_bound =
      linear.offset() + linear.num_finite_buckets() * linear.width();
  double lower_bound = linear.offset();

  int bucket_index;
  if (value < lower_bound || std::isnan(value)) {
    bucket_index = 0;
  } else if (value >= upper_bound) {
    bucket_index = linear.num_finite_buckets() + 1;
  } else {
    bucket_index = 1 + static_cast<int>((value - lower_bound) / linear.width());
  }

  distribution->set_bucket_counts(
      bucket_index, distribution->bucket_counts(bucket_index) + 1);
}

void UpdateExplicitBucketCount(double value, Distribution* distribution) {
  const auto& bounds = distribution->explicit_buckets().bounds();
  int bucket_index = 0;
  if (value >= bounds.Get(0)) {
    // -inf <  b0 <  b1 <  b2 <  b3 < +inf     (4 values in "bounds")
    //   |  0  |  1  |  2  |  3  |  4  |       (5 buckets)
    // std::upper_bound returns the first value that is greater than given one.
    bucket_index = std::distance(
        bounds.begin(), std::upper_bound(bounds.begin(), bounds.end(), value));
  }
  distribution->set_bucket_counts(
      bucket_index, distribution->bucket_counts(bucket_index) + 1);
}

}  // namespace

Status DistributionHelper::InitExponential(int num_finite_buckets,
                                           double growth_factor, double scale,
                                           Distribution* distribution) {
  if (num_finite_buckets <= 0) {
    return Status(Code::INVALID_ARGUMENT,
                  StrCat("Argument num_finite_buckets should be > 0. pass in: ",
                         num_finite_buckets));
  }
  if (growth_factor <= 1.0) {
    return Status(Code::INVALID_ARGUMENT,
                  StrCat("Argument growth_factor should be > 1.0. pass in: ",
                         growth_factor));
  }
  if (scale <= 0) {
    return Status(Code::INVALID_ARGUMENT,
                  StrCat("Argument scale should be > 0. pass in: ", scale));
  }

  auto* exponential = distribution->mutable_exponential_buckets();
  exponential->set_num_finite_buckets(num_finite_buckets);
  exponential->set_growth_factor(growth_factor);
  exponential->set_scale(scale);
  distribution->mutable_bucket_counts()->Resize(num_finite_buckets + 2, 0);
  return Status::OK;
}

Status DistributionHelper::InitLinear(int num_finite_buckets, double width,
                                      double offset,
                                      Distribution* distribution) {
  if (num_finite_buckets <= 0) {
    return Status(Code::INVALID_ARGUMENT,
                  StrCat("Argument num_finite_buckets should be > 0. pass in: ",
                         num_finite_buckets));
  }
  if (width <= 0.0) {
    return Status(Code::INVALID_ARGUMENT,
                  StrCat("Argument width should be > 0.0. pass in: ", width));
  }

  auto* linear = distribution->mutable_linear_buckets();
  linear->set_num_finite_buckets(num_finite_buckets);
  linear->set_width(width);
  linear->set_offset(offset);
  distribution->mutable_bucket_counts()->Resize(num_finite_buckets + 2, 0);
  return Status::OK;
}

Status DistributionHelper::InitExplicit(const std::vector<double>& bounds,
                                        Distribution* distribution) {
  if (!std::is_sorted(bounds.begin(), bounds.end())) {
    return Status(Code::INVALID_ARGUMENT, "Argument bounds should be sorted.");
  }
  if (std::adjacent_find(bounds.begin(), bounds.end()) != bounds.end()) {
    return Status(
        Code::INVALID_ARGUMENT,
        "Two adjacent elements in argument bounds should NOT be the same.");
  }

  auto* explicit_buckets = distribution->mutable_explicit_buckets();
  for (unsigned int i = 0; i < bounds.size(); ++i) {
    explicit_buckets->mutable_bounds()->Add(bounds.at(i));
  }
  distribution->mutable_bucket_counts()->Resize(bounds.size() + 1, 0);
  return Status::OK;
}

Status DistributionHelper::AddSample(double value, Distribution* distribution) {
  switch (distribution->bucket_option_case()) {
    case Distribution::kExponentialBuckets:
      UpdateGeneralStatictics(value, distribution);
      UpdateExponentialBucketCount(value, distribution);
      break;
    case Distribution::kLinearBuckets:
      UpdateGeneralStatictics(value, distribution);
      UpdateLinearBucketCount(value, distribution);
      break;
    case Distribution::kExplicitBuckets:
      UpdateGeneralStatictics(value, distribution);
      UpdateExplicitBucketCount(value, distribution);
      break;
    default:
      return Status(Code::INVALID_ARGUMENT,
                    StrCat("Unknown bucket option case: ",
                           distribution->bucket_option_case()));
  }
  return Status::OK;
}

Status DistributionHelper::Merge(const Distribution& from, Distribution* to) {
  if (!BucketsApproximatelyEqual(from, *to)) {
    return Status(Code::INVALID_ARGUMENT,
                  std::string("Bucket options don't match. From: ") +
                      from.DebugString() + " to: " + to->DebugString());
  }

  // TODO(chengliang): Make merging more tolerant here.
  if (from.bucket_counts_size() != to->bucket_counts_size()) {
    return Status(Code::INVALID_ARGUMENT, "Bucket counts size don't match.");
  }

  if (from.count() <= 0) return Status::OK;
  if (to->count() <= 0) {
    *to = from;
    return Status::OK;
  }

  int64_t count = to->count();
  double mean = to->mean();
  double sum_of_squared_deviation = to->sum_of_squared_deviation();

  to->set_count(to->count() + from.count());
  to->set_minimum(std::min(from.minimum(), to->minimum()));
  to->set_maximum(std::max(from.maximum(), to->maximum()));
  to->set_mean((count * mean + from.count() * from.mean()) / to->count());
  to->set_sum_of_squared_deviation(
      sum_of_squared_deviation + from.sum_of_squared_deviation() +
      count * (to->mean() - mean) * (to->mean() - mean) +
      from.count() * (to->mean() - from.mean()) * (to->mean() - from.mean()));

  for (int i = 0; i < from.bucket_counts_size(); i++) {
    to->set_bucket_counts(i, to->bucket_counts(i) + from.bucket_counts(i));
  }
  return Status::OK;
}

}  // namespace service_control_client
}  // namespace google
