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

git_repository(
    name = "boringssl_git",
    commit = "f7cc893d5032d11ae32646f93ace1c1237b9f463",  # 2016-07-07
    remote = "https://boringssl.googlesource.com/boringssl",
)

bind(
    name = "boringssl_crypto",
    actual = "@boringssl_git//:crypto",
)
