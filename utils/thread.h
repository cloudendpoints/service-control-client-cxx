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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_THREAD_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_THREAD_H_

#include "google/protobuf/stubs/status.h"

#include <future>
#include <mutex>
#include <thread>

namespace google {
namespace service_control_client {

// Put all thread related dependencies in this header.
// So they can be switched to use different packages.
typedef std::mutex Mutex;
typedef std::unique_lock<Mutex> MutexLock;

typedef std::future<::google::protobuf::util::Status> StatusFuture;
typedef std::promise<::google::protobuf::util::Status> StatusPromise;

typedef std::thread Thread;

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_UTILS_THREAD_H_
