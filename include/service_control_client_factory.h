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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_H_

#include "service_control_client.h"

namespace google {
namespace service_control_client {

// A factory to create ServiceControlClients.
// It does not provide any additional functionality other than making code
// testable/mockable via dependency injection.
class ServiceControlClientFactory {
 public:
  virtual ~ServiceControlClientFactory() {}

  // Create a `ServiceControlClient` object.
  virtual std::unique_ptr<ServiceControlClient> CreateClient(
      const std::string& service_name, const std::string& service_config_id,
    ServiceControlClientOptions& options) const = 0;

  // ServiceControlClientFactory is neither copyable nor movable.
  ServiceControlClientFactory(const ServiceControlClientFactory&) = delete;
  ServiceControlClientFactory& operator=(const ServiceControlClientFactory&) =
      delete;

 protected:
  ServiceControlClientFactory() {}
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_H_
