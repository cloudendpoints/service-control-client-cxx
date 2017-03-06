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

#include "include/service_control_client.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "utils/status_test_util.h"
#include "utils/thread.h"

#include <vector>

using std::string;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::_;

namespace google {
namespace service_control_client {


}  // namespace service_control_client
}  // namespace google
