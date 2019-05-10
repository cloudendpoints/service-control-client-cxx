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
    commit = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
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

git_repository(
    name = "boringssl",
    commit = "5b8bd1ba221804c81c8a92c6d1d353ef43a851ab",  # 2018-12-14
    remote = "https://boringssl.googlesource.com/boringssl",
)

bind(
    name = "boringssl_crypto",
    actual = "@boringssl//:crypto",
)

git_repository(
    name = "protobuf_git",
    commit = "48cb18e5c419ddd23d9badcfe4e9df7bde1979b2",  # same as grpc
    remote = "https://github.com/protocolbuffers/protobuf.git",
)

bind(
    name = "protoc",
    actual = "@protobuf_git//:protoc",
)

bind(
    name = "protobuf",
    actual = "@protobuf_git//:protobuf",
)

bind(
    name = "cc_wkt_protos",
    actual = "@protobuf_git//:cc_wkt_protos",
)

bind(
    name = "cc_wkt_protos_genproto",
    actual = "@protobuf_git//:cc_wkt_protos_genproto",
)

new_git_repository(
    name = "googleapis_git",
    commit = "620947d5dc99cab4f255474a85e504433a52d8a5",
    remote = "https://github.com/googleapis/googleapis.git",
    build_file = "third_party/BUILD.googleapis",
)

bind(
    name = "servicecontrol",
    actual = "@googleapis_git//:servicecontrol",
)

bind(
    name = "servicecontrol_genproto",
    actual = "@googleapis_git//:servicecontrol_genproto",
)

bind(
    name = "service_config",
    actual = "@googleapis_git//:service_config",
)

