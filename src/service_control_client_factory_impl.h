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

#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_IMPL_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_IMPL_H_

#include "include/service_control_client_factory.h"
#include "include/service_control_client.h"

namespace google {
namespace service_control_client {

class ServiceControlClientFactoryImpl : public ServiceControlClientFactory {
 public:
   ServiceControlClientFactoryImpl() {}
  ~ServiceControlClientFactoryImpl() override {}

  std::unique_ptr<ServiceControlClient> CreateClient(
      const std::string& service_name, const std::string& service_config_id,
    ServiceControlClientOptions& options) const override {
     return CreateServiceControlClient(service_name, service_config_id, options);
   }
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_SERVICE_CONTROL_CLIENT_FACTORY_IMPL_H_
