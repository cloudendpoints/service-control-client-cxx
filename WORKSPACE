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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "googletest_git",
    build_file = "@//:third_party/BUILD.googletest",
    sha256 = "01508c8f47c99509130f128924f07f3a60be05d039cff571bb11d60bb11a3581",
    strip_prefix = "googletest-d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
    urls = ["https://github.com/google/googletest/archive/d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0.tar.gz"],
)

http_archive(
    name = "boringssl",
    sha256 = "4825306f702fa5cb76fd86c987a88c9bbb241e75f4d86dbb3714530ca73c1fb1",
    strip_prefix = "boringssl-8cb07520451f0dc454654f2da5cdecf0b806f823",
    urls = ["https://github.com/google/boringssl/archive/8cb07520451f0dc454654f2da5cdecf0b806f823.tar.gz"],
)


http_archive(
    name = "protobuf_git",
    strip_prefix = "protobuf-d5f0dac497f833d06f92d246431f4f2f42509e04",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/d5f0dac497f833d06f92d246431f4f2f42509e04.tar.gz"],
    sha256 = "6514bc2994b29c2571ad2ceb90da25b7fb0dd575f5830fe109ddb214d59e3e2b",
)

# Required by protobuf_git
http_archive(
    name = "bazel_skylib",
    sha256 = "bbccf674aa441c266df9894182d80de104cabd19be98be002f6d478aaa31574d",
    strip_prefix = "bazel-skylib-2169ae1c374aab4a09aa90e65efe1a3aad4e279b",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/2169ae1c374aab4a09aa90e65efe1a3aad4e279b.tar.gz"],
)

http_archive(
    name = "googleapis_git",
    build_file = "@//:third_party/BUILD.googleapis",
    patch_cmds = ["find . -type f -name '*BUILD*' | xargs rm"],
    strip_prefix = "googleapis-275cdfcdc3188a60456f43acd139b8cc037379f4",  # May 14, 2019
    url = "https://github.com/googleapis/googleapis/archive/275cdfcdc3188a60456f43acd139b8cc037379f4.tar.gz",
    sha256 = "d07a9bf06bb02b51ff6e913211cedc7511430af550b6a775908c33c8ee218985",
)

