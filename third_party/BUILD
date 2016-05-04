# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Reimplementation of error table generator Boring SSL uses in build.
# Boring SSL implementation is in go which doesn't yet have complete Bazel
# support and the temporary support used in nginx workspace
# https://nginx.googlesource.com/workspace does not work well with
# Bazel sandboxing. Therefore, we temporarily reimplement the error
# table generator.

py_binary(
    name = "boringssl_error_gen",
    srcs = [
        "boringssl_error_gen.py",
    ],
    visibility = ["//visibility:public"],
)
