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

// Utility functions for google.type.Money.

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_MONEY_UTILS_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_MONEY_UTILS_H_

#include "google/protobuf/stubs/status.h"
#include "google/type/money.pb.h"

namespace google {
namespace service_control_client {

// Returns OK if the given money is a valid value. The possible validation
// errors include invalid currency_code format, nanos out of range, and
// the signs of units and nanos disagree. In all error cases the error
// code is INVALID_ARGUMENT, with an error message.
::google::protobuf::util::Status ValidateMoney(
    const ::google::type::Money& money);

// Returns 1 if the given money has a positive amount, 0 if money has zero
// amount, and -1 if money has a negative amount.
// The given money must be valid (see Validate) or the result may be wrong.
int GetAmountSign(const ::google::type::Money& money);

// Adds a and b together into sum. The caller owns the lifetime of sum. Both
// a and b must be valid money values (see ValidateMoney), otherwise sum may
// contain invalid value.
// Returns OK if successful. There are two possible errors:
// (1) If the currency_code of a and b are different, sum is cleared and
// INVALID_ARGUMENT is returned.
// (2) If arithmetic overflow occurs during the additions, sum is set to the
// maximum positive or minimum negative amount depending on the direction of
// the overflow, and OUT_OF_RANGE is returned.
::google::protobuf::util::Status TryAddMoney(const ::google::type::Money& a,
                                             const ::google::type::Money& b,
                                             ::google::type::Money* sum);

// Returns the sum of a and b. Both a and b must be valid money values (see
// ValidateMoney), otherwise the result may contain invalid value. The
// caller must ensure a and b have the same currency_code, otherwise it's a
// fatal error.
//
// If arithmetic overflow occurs during the addition, the return value is
// set to the maximum positive or minimum negative amount depending on the
// direction of the overflow.
::google::type::Money SaturatedAddMoney(const ::google::type::Money& a,
                                        const ::google::type::Money& b);

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_MONEY_UTILS_H_
