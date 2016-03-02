#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_

#include <functional>

namespace google {
namespace service_control_client {

// An interface to provide periodic timer for the library.
class PeriodicTimer {
 public:
  // Defines the function to be called periodicall by the timer.
  // timer_is_canceled indicates if the function is called when the caller
  // is canceled. All calls should be true except the last one when the
  // timer is canceled.
  using PeriodicCallback = std::function<void(bool timer_is_canceled)>;

  // Represents a timer instance created by StartTimer.
  // Its only purpose is to cancel the timer instance.
  class TimerInstance {
    // Destructor
    virtual ~TimerInstance() {}

    // Cancel the timer instance.
    virtual void Cancel() = 0;
  };

  // Destructor
  virtual ~PeriodicTimer() {}

  // Creates a periodic timer instance which will call the callback
  // with desired interval. The returned object can be used to cancel
  // the instance.
  virtual std::unique_ptr<TimerInstance> StartTimer(
      int interval_ms, PeriodicCallback callback) = 0;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_
