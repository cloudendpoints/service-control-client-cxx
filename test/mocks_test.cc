/* Copyright 2021 Google Inc. All Rights Reserved.

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

#include "mocks.h"
#include "gtest/gtest.h"

namespace google {
namespace service_control_client {
namespace {

// Simple unit tests to ensure mocks compile (all virtual methods are mocked).
TEST(MocksTest, MocksCompile) {
  testing::MockServiceControlClientFactory mock_factory;
  testing::MockServiceControlClient mock_client;
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
