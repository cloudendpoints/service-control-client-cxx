#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_

#include <functional>
#include <memory>

namespace google {
namespace service_control_client {

// An interface to provide periodic timer for the library.
class PeriodicTimer {
 public:
  // Defines the function to be called periodically by the timer.
  // timer_is_canceled indicates if the function is called when the timer
  // is being canceled. It should be true for all calls except the last
  // one when the timer is being canceled.
  using PeriodicCallback = std::function<void()>;

  // Represents a timer created by StartTimer.
  // Its only purpose is to cancel the timer instance.
  class Timer {
   public:
    // Destructor
    virtual ~Timer() {}

    // Cancels the timer.
    virtual void Stop() = 0;
  };

  // Destructor
  virtual ~PeriodicTimer() {}

  // Creates a periodic timer periodically calling the callback
  // with desired interval. The returned object can be used to cancel
  // the instance.
  virtual std::unique_ptr<Timer> StartTimer(int interval_ms,
                                            PeriodicCallback callback) = 0;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_PERIODIC_TIMER_H_
