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
    sha256 = "f9830bcfb68c8a2a689337c4ad6998d6849df9574451f338a3dde14ff1f3e381",
    strip_prefix = "googletest-23b2a3b1cf803999fb38175f6e9e038a4495c8a5",  # 2/13/2020
    urls = ["https://github.com/google/googletest/archive/23b2a3b1cf803999fb38175f6e9e038a4495c8a5.tar.gz"],
)

http_archive(
    name = "boringssl",
    sha256 = "4825306f702fa5cb76fd86c987a88c9bbb241e75f4d86dbb3714530ca73c1fb1",
    strip_prefix = "boringssl-8cb07520451f0dc454654f2da5cdecf0b806f823",
    urls = ["https://github.com/google/boringssl/archive/8cb07520451f0dc454654f2da5cdecf0b806f823.tar.gz"],
)

# Required by com_google_protobuf
http_archive(
    name = "rules_python",
    sha256 = "e5470e92a18aa51830db99a4d9c492cc613761d5bdb7131c04bd92b9834380f6",
    strip_prefix = "rules_python-4b84ad270387a7c439ebdccfd530e2339601ef27",  # 8/2/2019
    urls = ["https://github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz"],
)

# Required by com_google_protobuf
http_archive(
    name = "bazel_skylib",
    sha256 = "b6cddd8206d5d2953791398b0f025a3f3f3c997872943625529e7b30eba92e78",
    strip_prefix = "bazel-skylib-c6f6b5425b232baf5caecc3aae31d49d63ddec03",  # 06/09/2021
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/c6f6b5425b232baf5caecc3aae31d49d63ddec03.tar.gz"],
)

# Required by com_google_protobuf
http_archive(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff",
    strip_prefix = "zlib-1.2.11",
    urls = ["https://github.com/madler/zlib/archive/v1.2.11.tar.gz"],
)

http_archive(
    name = "com_google_protobuf",
    sha256 = "236df7c01ca5d3633f2086fa98dbefcbdca638a4ac0ecd9f6e6aab4e629c013f",
    strip_prefix = "protobuf-b24d0c2b7aeb2923d6e8e0c23946e7e2f493053b",  # 06/09/2021
    urls = ["https://github.com/protocolbuffers/protobuf/archive/b24d0c2b7aeb2923d6e8e0c23946e7e2f493053b.tar.gz"],
)

http_archive(
    name = "googleapis_git",
    sha256 = "0eaf8c4d0ea4aa3ebf94bc8f5ec57403c633920ada57a498fea4a8eb8c17b948",
    strip_prefix = "googleapis-68c72c1d1ffff49b7d0019a21e65705b5d9c23c2",  # 06/04/2020
    url = "https://github.com/googleapis/googleapis/archive/68c72c1d1ffff49b7d0019a21e65705b5d9c23c2.tar.gz",
)

load("@googleapis_git//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
)
