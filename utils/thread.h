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
