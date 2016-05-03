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

#include <limits>

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

using ::google::api::servicecontrol::v1::Distribution;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

namespace google {
namespace service_control_client {
namespace {

const char kInitExponentialDistribution[] = R"(
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 0.001
}
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
)";

const char kInitLinearDistribution[] = R"(
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 1
}
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
)";

// Same bucket range as the linear distribution above.
const char kInitExplicitDistribution[] = R"(
explicit_buckets {
  bounds: 1.0
  bounds: 3.0
  bounds: 5.0
}
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
)";

const double kOneValueExponential = 0.001;
const char kOneValueExponentialDistribution[] = R"(
count: 1
mean: 0.001
sum_of_squared_deviation: 0
minimum: 0.001
maximum: 0.001
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 0.001
}
bucket_counts: 0
bucket_counts: 1
bucket_counts: 0
bucket_counts: 0
)";

const double kOneValueLinear = 3.5;
const char kOneValueLinearDistribution[] = R"(
count: 1
mean: 3.5
sum_of_squared_deviation: 0
minimum: 3.5
maximum: 3.5
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 1
}
bucket_counts: 0
bucket_counts: 0
bucket_counts: 1
bucket_counts: 0
)";

const double kOneValueExplicit = 7.5;
const char kOneValueExplicitDistribution[] = R"(
count: 1
mean: 7.5
sum_of_squared_deviation: 0
minimum: 7.5
maximum: 7.5
explicit_buckets {
  bounds: 1.0
  bounds: 3.0
  bounds: 5.0
}
bucket_counts: 0
bucket_counts: 0
bucket_counts: 0
bucket_counts: 1
)";

const double kTwoValuesExponential[] = {0.001, 0.002};
const char kTwoValuesExponentialDistribution[] = R"(
count: 2
mean: 0.0015
sum_of_squared_deviation: 5.0e-07
minimum: 0.001
maximum: 0.002
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 0.001
}
bucket_counts: 0
bucket_counts: 1
bucket_counts: 1
bucket_counts: 0
)";

const double kTwoValuesLinear[] = {1.5, 3.5};
const char kTwoValuesLinearDistribution[] = R"(
count: 2
mean: 2.5
sum_of_squared_deviation: 2.0
minimum: 1.5
maximum: 3.5
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 1
}
bucket_counts: 0
bucket_counts: 1
bucket_counts: 1
bucket_counts: 0
)";

const double kTwoValuesExplicit[] = {0.5, 2.5};
const char kTwoValuesExplicitDistribution[] = R"(
count: 2
mean: 1.5
sum_of_squared_deviation: 2.0
minimum: 0.5
maximum: 2.5
explicit_buckets {
  bounds: 1.0
  bounds: 3.0
  bounds: 5.0
}
bucket_counts: 1
bucket_counts: 1
bucket_counts: 0
bucket_counts: 0
)";

const double kMultipleValuesExponential[] = {-1,    0,      0.001, 0.0015,
                                             0.002, 0.0025, 0.004, 0.005};
const char kMultipleValuesExponentialDistribution[] = R"(
count: 8
mean: -0.123
sum_of_squared_deviation: 0.8790225
minimum: -1
maximum: 0.005
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 0.001
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

const double kMultipleValuesLinear[] = {-5,    1.36,  0.69, 2.78,
                                        3.456, 4.809, 5.92, 10.23};
const char kMultipleValuesLinearDistribution[] = R"(
count: 8
mean: 3.030625
sum_of_squared_deviation: 136.346313875
minimum: -5
maximum: 10.23
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 1
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

const double kMultipleValuesExplicit[] = {-0.5, 0.5, 1.5, 2.5,
                                          3.5,  4.5, 5.5, 6.5};
const char kMultipleValuesExplicitDistribution[] = R"(
count: 8
mean: 3.0
sum_of_squared_deviation: 42.0
minimum: -0.5
maximum: 6.5
explicit_buckets {
  bounds: 1.0
  bounds: 3.0
  bounds: 5.0
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

// Special value Nan comparison is not supported by
// google/protobuf/util/message_differencer.h.
// following tests have to be disabled before it is supported.
#define TREAT_NAN_AS_EQUAL_SUPPORT 0

#if TREAT_NAN_AS_EQUAL_SUPPORT
const double kSpecialValues[] = {
    +0,
    -0,
    std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(),
    std::numeric_limits<double>::quiet_NaN(),
};

const char kSpecialValuesExponentialDistribution[] = R"(
count: 5
mean: nan
sum_of_squared_deviation: nan
minimum: nan
maximum: nan
exponential_buckets {
  num_finite_buckets: 2
  growth_factor: 2
  scale: 0.001
}
bucket_counts: 4
bucket_counts: 0
bucket_counts: 0
bucket_counts: 1
)";

const char kSpecialValuesLinearDistribution[] = R"(
count: 5
mean: nan
sum_of_squared_deviation: nan
minimum: nan
maximum: nan
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 1
}
bucket_counts: 4
bucket_counts: 0
bucket_counts: 0
bucket_counts: 1
)";

const char kSpecialValuesExplicitDistribution[] = R"(
count: 5
mean: nan
sum_of_squared_deviation: nan
minimum: nan
maximum: nan
explicit_buckets {
  bounds: 1.0
  bounds: 3.0
  bounds: 5.0
}
bucket_counts: 4
bucket_counts: 0
bucket_counts: 0
bucket_counts: 1
)";
#endif  // TREAT_NAN_AS_EQUAL_SUPPORT

// Distribution for [-1, 1, 3, 5]
const char kLinearDistribution[] = R"(
count: 4
mean: 2
sum_of_squared_deviation: 20
minimum: -1
maximum: 5
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 0
}
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
)";

// Combined Distribution for [-1, 1, 3, 5] and [-1, 1, 3, 5].
const char kCombinedLinearDistribution[] = R"(
count: 8
mean: 2
sum_of_squared_deviation: 40
minimum: -1
maximum: 5
linear_buckets {
  num_finite_buckets: 2
  width: 2
  offset: 0
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

// Distribution for [-1, 1, 3, 5]
const char kExponentialDistribution[] = R"(
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

// Combined distribution for [-1, 1, 3, 5] and [-1, 1, 3, 5]
const char kCombinedExponentialDistribution[] = R"(
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

// Distribution for [-1, 1, 3, 5]
const char kExplicitDistribution[] = R"(
count: 4
mean: 2
sum_of_squared_deviation: 20
minimum: -1
maximum: 5
explicit_buckets {
  bounds: 0
  bounds: 2
  bounds: 4
}
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
bucket_counts: 1
)";

// Combined Distribution for [-1, 1, 3, 5] and [-1, 1, 3, 5].
const char kCombinedExplicitDistribution[] = R"(
count: 8
mean: 2
sum_of_squared_deviation: 40
minimum: -1
maximum: 5
explicit_buckets {
  bounds: 0
  bounds: 2
  bounds: 4
}
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
bucket_counts: 2
)";

class DistributionHelperTest : public ::testing::Test {
 protected:
  DistributionHelperTest() {
    helper_.InitExponential(2 /* num_finite_buckets */, 2 /* growth_factor */,
                            0.001 /* scale */, &exponential_distribution_);
    other_exponential_distribution_ = exponential_distribution_;

    helper_.InitLinear(2 /* num_finite_buckets */, 2 /* width */,
                       1 /* offset */, &linear_distribution_);
    helper_.InitExplicit({1.0, 3.0, 5.0}, &explicit_distribution_);
  }

  DistributionHelper helper_;

  Distribution exponential_distribution_;
  Distribution linear_distribution_;
  Distribution explicit_distribution_;

  Distribution other_exponential_distribution_;
};

TEST_F(DistributionHelperTest, InitializeDistribution_Exponential) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kInitExponentialDistribution, &expected));
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, InitializeDistribution_Linear) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kInitLinearDistribution, &expected));
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(linear_distribution_, expected));
}

TEST_F(DistributionHelperTest, InitializeDistribution_Explicit) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kInitExplicitDistribution, &expected));
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(explicit_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_OneValue_Exponential) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kOneValueExponentialDistribution, &expected));
  helper_.AddSample(kOneValueExponential, &exponential_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_OneValue_Linear) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kOneValueLinearDistribution, &expected));
  helper_.AddSample(kOneValueLinear, &linear_distribution_);
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(linear_distribution_, expected));
}

TEST_F(DistributionHelperTest, AddSample_OneValue_Explicit) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kOneValueExplicitDistribution, &expected));
  helper_.AddSample(kOneValueExplicit, &explicit_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(explicit_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_TwoValues_Exponential) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTwoValuesExponentialDistribution,
                                          &expected));
  for (double value : kTwoValuesExponential) {
    helper_.AddSample(value, &exponential_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_TwoValues_Linear) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTwoValuesLinearDistribution, &expected));
  for (double value : kTwoValuesLinear) {
    helper_.AddSample(value, &linear_distribution_);
  }
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(linear_distribution_, expected));
}

TEST_F(DistributionHelperTest, AddSample_TwoValues_Explicit) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTwoValuesExplicitDistribution, &expected));
  for (double value : kTwoValuesExplicit) {
    helper_.AddSample(value, &explicit_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(explicit_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_MultipleValues_Exponential) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      kMultipleValuesExponentialDistribution, &expected));
  for (double value : kMultipleValuesExponential) {
    helper_.AddSample(value, &exponential_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_MultipleValues_Linear) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kMultipleValuesLinearDistribution,
                                          &expected));
  for (double value : kMultipleValuesLinear) {
    helper_.AddSample(value, &linear_distribution_);
  }
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(linear_distribution_, expected));
}

TEST_F(DistributionHelperTest, AddSample_MultipleValues_Explicit) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kMultipleValuesExplicitDistribution,
                                          &expected));
  for (double value : kMultipleValuesExplicit) {
    helper_.AddSample(value, &explicit_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(explicit_distribution_,
                                                      expected));
}

#if TREAT_NAN_AS_EQUAL_SUPPORT
TEST_F(DistributionHelperTest, AddSample_SpecialValues_Exponential) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kSpecialValuesExponentialDistribution,
                                          &expected));
  for (double value : kSpecialValues) {
    helper_.AddSample(value, &exponential_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, AddSample_SpecialValues_Linear) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kSpecialValuesLinearDistribution, &expected));
  for (double value : kSpecialValues) {
    helper_.AddSample(value, &linear_distribution_);
  }
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(linear_distribution_, expected));
}

TEST_F(DistributionHelperTest, AddSample_SpecialValues_Explicit) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kSpecialValuesExplicitDistribution,
                                          &expected));
  for (double value : kSpecialValues) {
    helper_.AddSample(value, &explicit_distribution_);
  }
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(explicit_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, Merge_TwoDistributions_SpecialValues) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kSpecialValuesExponentialDistribution,
                                          &expected));

  int total_values = sizeof(kSpecialValues) / sizeof(double);
  for (int i = 0; i < total_values / 2; ++i) {
    helper_.AddSample(kSpecialValues[i], &exponential_distribution_);
  }

  for (int i = total_values / 2; i < total_values; ++i) {
    helper_.AddSample(kSpecialValues[i], &other_exponential_distribution_);
  }

  helper_.Merge(other_exponential_distribution_, &exponential_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}
#endif  // TREAT_NAN_AS_EQUAL_SUPPORT

TEST_F(DistributionHelperTest, AddSample_UnknownDistribution) {
  Distribution distribution;
  Distribution expected;
  helper_.AddSample(1, &distribution);
  EXPECT_TRUE(MessageDifferencer::Equals(distribution, expected));
}

// Merge does not care about bucket_option, so we only use exponential bucket
// in tests.
TEST_F(DistributionHelperTest, Merge_EmptyDistributions) {
  Distribution empty;
  helper_.InitExponential(2, 2, 0.001, &empty);
  Distribution other_empty = empty;

  helper_.Merge(other_empty, &empty);
  EXPECT_EQ(0, empty.count());
}

TEST_F(DistributionHelperTest, Merge_FromEmptyDistribution) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kOneValueExponentialDistribution, &expected));

  helper_.AddSample(kOneValueExponential, &exponential_distribution_);
  helper_.Merge(other_exponential_distribution_, &exponential_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, Merge_ToEmptyDistribution) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kOneValueExponentialDistribution, &expected));

  helper_.AddSample(kOneValueExponential, &exponential_distribution_);
  helper_.Merge(other_exponential_distribution_, &exponential_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, Merge_TwoDistributions) {
  Distribution expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      kMultipleValuesExponentialDistribution, &expected));

  int total_values = sizeof(kMultipleValuesExponential) / sizeof(double);
  for (int i = 0; i < total_values / 2; ++i) {
    helper_.AddSample(kMultipleValuesExponential[i],
                      &exponential_distribution_);
  }

  for (int i = total_values / 2; i < total_values; ++i) {
    helper_.AddSample(kMultipleValuesExponential[i],
                      &other_exponential_distribution_);
  }

  helper_.Merge(other_exponential_distribution_, &exponential_distribution_);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential_distribution_,
                                                      expected));
}

TEST_F(DistributionHelperTest, Merge_BucketMatch_Linear) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kCombinedLinearDistribution, &expected));

  Distribution distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &distribution));
  Distribution other_distribution = distribution;

  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest,
       Merge_BucketNotMatch_Linear_DifferentNumBuckets) {
  Distribution distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_linear_buckets()->set_num_finite_buckets(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Linear_DifferentWidth) {
  Distribution distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_linear_buckets()->set_width(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Linear_DifferentOffset) {
  Distribution distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_linear_buckets()->set_offset(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Explicit) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kCombinedExplicitDistribution, &expected));

  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExplicitDistribution, &distribution));
  Distribution other_distribution = distribution;

  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest,
       Merge_BucketNotMatch_Explicit_DifferentNumBuckets) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExplicitDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_explicit_buckets()->clear_bounds();

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Explicit_DifferentBounds) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExplicitDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_explicit_buckets()->set_bounds(0, 1.5);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketMatch_Exponential) {
  Distribution expected;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kCombinedExponentialDistribution, &expected));

  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));
  Distribution other_distribution = distribution;

  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest,
       Merge_BucketNotMatch_Exponential_DifferentNumBuckets) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_exponential_buckets()->set_num_finite_buckets(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest,
       Merge_BucketNotMatch_Exponential_DifferentGrowthFactor) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_exponential_buckets()->set_growth_factor(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest,
       Merge_BucketNotMatch_Exponential_DifferentScale) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));
  Distribution other_distribution = distribution;
  other_distribution.mutable_exponential_buckets()->set_scale(10);

  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Linear_Exponential) {
  Distribution linear;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &linear));

  Distribution exponential;
  ASSERT_TRUE(TextFormat::ParseFromString(kTwoValuesExponentialDistribution,
                                          &exponential));

  Distribution expected = exponential;
  helper_.Merge(linear, &exponential);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(exponential, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Explicit_Linear) {
  Distribution linear;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &linear));

  Distribution explicit_distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kTwoValuesExponentialDistribution,
                                          &explicit_distribution));

  Distribution expected = linear;
  helper_.Merge(explicit_distribution, &linear);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(linear, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_Exponential_Explicit) {
  Distribution exponential;
  ASSERT_TRUE(TextFormat::ParseFromString(kLinearDistribution, &exponential));

  Distribution explicit_distribution;
  ASSERT_TRUE(TextFormat::ParseFromString(kTwoValuesExponentialDistribution,
                                          &explicit_distribution));

  Distribution expected = explicit_distribution;
  helper_.Merge(exponential, &explicit_distribution);
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(explicit_distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_BucketNotMatch_UnknownBucketOptions) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));
  Distribution other_distribution = distribution;

  // Both distribution have default BUCKET_TYPE_NOT_SET bucket_option.
  distribution.clear_linear_buckets();
  distribution.clear_exponential_buckets();
  distribution.clear_explicit_buckets();
  other_distribution.clear_linear_buckets();
  other_distribution.clear_exponential_buckets();
  other_distribution.clear_explicit_buckets();

  // The Merge function should not break in this situation.
  Distribution expected = distribution;
  helper_.Merge(other_distribution, &distribution);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(distribution, expected));
}

TEST_F(DistributionHelperTest, Merge_DifferentBucketCountsSize) {
  Distribution from = exponential_distribution_;
  from.mutable_bucket_counts()->Resize(5, 0);
  from.set_bucket_counts(1, 10);
  Distribution to = exponential_distribution_;

  // The merge won't happen because the bucket size are different.
  helper_.Merge(from, &to);
  EXPECT_TRUE(
      MessageDifferencer::ApproximatelyEquals(to, exponential_distribution_));
}

TEST_F(DistributionHelperTest, Merge_ZeroCount) {
  Distribution distribution;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExponentialDistribution, &distribution));

  Distribution to = distribution;
  to.set_count(0);

  helper_.Merge(distribution, &to);
  EXPECT_TRUE(MessageDifferencer::ApproximatelyEquals(to, distribution));
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
