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

#include "src/money_utils.h"

using google::type::Money;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace service_control_client {

Status ValidateMoney(const Money& money) {
  if (money.currency_code().empty() || money.currency_code().length() != 3) {
    return Status(Code::INVALID_ARGUMENT,
                  "The currency_code field in Money must be 3 letters long.");
  }
  if ((money.units() > 0 && money.nanos() < 0) ||
      (money.units() < 0 && money.nanos() > 0)) {
    return Status(Code::INVALID_ARGUMENT,
                  "The signs of units and nanos field in Money must agree.");
  }
  const int kMaxNanos = 999999999;
  if (money.nanos() < -kMaxNanos || money.nanos() > kMaxNanos) {
    return Status(Code::INVALID_ARGUMENT,
                  "The nanos field in Money must be between -999999999 and "
                  "999999999 inclusive.");
  }
  return Status::OK;
}

int GetAmountSign(const Money& money) {
  if (money.units() > 0) {
    return 1;
  } else if (money.units() < 0) {
    return -1;
  } else if (money.nanos() > 0) {
    return 1;
  } else if (money.nanos() < 0) {
    return -1;
  } else {
    return 0;
  }
}

Status TryAddMoney(const Money& a, const Money& b, Money* sum) {
  if (a.currency_code() != b.currency_code()) {
    sum->clear_currency_code();
    sum->clear_units();
    sum->clear_nanos();
    return Status(Code::INVALID_ARGUMENT,
                  "Money values must have the same currency_code to add");
  }
  sum->set_currency_code(a.currency_code());

  int carry = 0;  // The unit value carried from adding nanos

  // Calculate the sum of nanos.
  int sum_nanos = a.nanos() + b.nanos();
  const int kBillion = 1000000000;
  if (sum_nanos >= kBillion) {
    carry = 1;
    sum_nanos -= kBillion;
  } else if (sum_nanos <= -kBillion) {
    carry = -1;
    sum_nanos += kBillion;
  }

  // Calculate the sum of units.
  int64_t sum_units_no_carry = a.units() + b.units();
  int64_t sum_units = sum_units_no_carry + carry;
  // It's possible the sum_units and sum_nanos now have different signs,
  // for example a.units=-2, a.nanos=-7, b.units=5, b.nanos=3. Adjust if
  // necessary.
  if (sum_units > 0 && sum_nanos < 0) {
    --sum_units;
    sum_nanos += kBillion;
  } else if (sum_units < 0 && sum_nanos > 0) {
    ++sum_units;
    sum_nanos -= kBillion;
  }

  // Get the sign of a and b in order to detect overflow.
  int sign_a = GetAmountSign(a);
  int sign_b = GetAmountSign(b);

  // Detect positive overflow.
  if (sign_a > 0 && sign_b > 0 && sum_units <= 0) {
    sum->set_units(INT64_MAX);
    sum->set_nanos(kBillion - 1);
    return Status(Code::OUT_OF_RANGE, "Money addition positive overflow");
  }
  // Detect negative overflow. Note there is a tricky case that can only happen
  // to negative overflow: sum_units_no_carry overflows to 0 but adding the
  // carry makes sum_units a negative number again. So we must check both
  // sum_units_no_carry and sum_units here. This doesn't happen in the
  // positive overflow case because two MAX values 0x7FF...FF adds to
  // 0xFF...FE and the carry won't affect the sign.
  if (sign_a < 0 && sign_b < 0 && (sum_units_no_carry >= 0 || sum_units >= 0)) {
    sum->set_units(INT64_MIN);
    sum->set_nanos(-kBillion + 1);
    return Status(Code::OUT_OF_RANGE, "Money addition negative overflow");
  }

  // The success case.
  sum->set_units(sum_units);
  sum->set_nanos(sum_nanos);
  return Status::OK;
}

Money SaturatedAddMoney(const Money& a, const Money& b) {
  Money sum;
  Status status = TryAddMoney(a, b, &sum);
  // Ignore overflow and crash on other errors.
  assert(status.ok() || status.error_code() == Code::OUT_OF_RANGE);
  return sum;
}

}  // namespace service_control_client
}  // namespace google
