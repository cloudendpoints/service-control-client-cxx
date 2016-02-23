#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_THREAD_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_THREAD_H_

#include <mutex>

namespace google {
namespace service_control_client {

// Put all thread related dependencies in this header.
// So they can be switched to use different packages.

typedef std::mutex Mutex;
typedef std::unique_lock<Mutex> MutexLock;

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_THREAD_H_
