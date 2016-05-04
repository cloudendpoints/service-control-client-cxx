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

# A Bazel (http://bazel.io) workspace for the Google Service Control client

new_git_repository(
    name = "googletest_git",
    build_file = "third_party/BUILD.googletest",
    commit = "13206d6f53aaff844f2d3595a01ac83a29e383db",
    remote = "https://github.com/google/googletest.git",
)

bind(
    name = "googletest",
    actual = "@googletest_git//:googletest",
)

bind(
    name = "googletest_main",
    actual = "@googletest_git//:googletest_main",
)

# Reimplementation of error table generator Boring SSL uses in build.
# Boring SSL implementation is in go which doesn't yet have complete Bazel
# support and the temporary support used in nginx workspace
# https://nginx.googlesource.com/workspace does not work well with
# Bazel sandboxing. Therefore, we temporarily reimplement the error
# table generator.
bind(
    name = "boringssl_error_gen",
    actual = "//third_party:boringssl_error_gen",
)

new_git_repository(
    name = "boringssl_git",
    build_file = "third_party/BUILD.boringssl",
    commit = "c4f25ce0c6e3822eb639b2a5649a51ea42b46490",
    remote = "https://boringssl.googlesource.com/boringssl",
)

bind(
    name = "boringssl_crypto",
    actual = "@boringssl_git//:crypto",
)
