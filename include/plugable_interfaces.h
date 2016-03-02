#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_PLUGABLE_INTERFACES_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_PLUGABLE_INTERFACES_H_

#include <functional>
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace service_control_client {

// Non blocking transport interface to talk to Service Control Server.
class NonBlockingTransport {
 public:
  using DoneCallback =
      std::function<void(const ::google::protobuf::util::Status&)>;

  // Destructor
  virtual ~NonBlockingTransport() {}

  // Sends check_request protobuf to service control server.
  // check_response is valid after on_check_done() is called and status is OK.
  virtual void Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response,
      DoneCallback on_check_done) = 0;

  // Sends report_request protobuf to service control server.
  // resport_response is valid after on_report_done() is called and status is
  // OK.
  virtual void Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response,
      DoneCallback on_report_done) = 0;
};

// Blocking transport interface to talk to Service Control Server.
class BlockingTransport {
 public:
  using DoneCallback =
      std::function<void(const ::google::protobuf::util::Status&)>;

  // Destructor
  virtual ~BlockingTransport() {}

  // Sends check_request protobuf to service control server.
  virtual ::google::protobuf::util::Status Check(
      const ::google::api::servicecontrol::v1::CheckRequest& check_request,
      ::google::api::servicecontrol::v1::CheckResponse* check_response) = 0;

  // Sends report_request protobuf to service control server.
  virtual ::google::protobuf::util::Status Report(
      const ::google::api::servicecontrol::v1::ReportRequest& report_request,
      ::google::api::servicecontrol::v1::ReportResponse* report_response) = 0;
};

// An interface to run a function in a separate thread.
class TaskRunner {
 public:
  virtual ~TaskRunner() {}

  // To run the function in a separate thread.
  virtual void Run(std::function<void()> func) = 0;
};

// An interface to call a function periodically.
class PeriodicTimer {
 public:
  // TimerID is just a pointer to underline object.
  typedef void* TimerID;

  virtual ~PeriodicTimer() {}

  // Starts a timer to call the func periodically with interval_ms.
  // Returns timer_id if sucessful.
  virtual ::google::protobuf::util::Status Start(int interval_ms,
                                                 std::function<void()> func,
                                                 TimerID* timer_id) = 0;

  // Stop a timer.
  virtual void Stop(TimerID timer_id) = 0;
};

// Creates a GRPC NonBlockingTransport object.
std::unique_ptr<NonBlockingTransport> CreateGrpcNonBlockingTransport(
    const std::string& service_control_grpc_server);

// Creates a GRPC BlockingTransport object.
std::unique_ptr<BlockingTransport> CreateGrpcBlockingTransport(
    const std::string& service_control_grpc_server);

// Creates a thread based PeriodicTimer by using std::thread
std::unique_ptr<PeriodicTimer> CreateThreadBasedPeriodicTimer();

// Creates a thread based TaskRunner by using std::thread
std::unique_ptr<PeriodicTimer> CreateThreadBasedTaskRunner();

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_PLUGABLE_INTERFACES_H_
