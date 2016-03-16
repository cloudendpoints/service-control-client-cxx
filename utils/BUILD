
licenses(["notice"])

cc_library(
    name = "common_headers",
    hdrs = [
         "google_macros.h",
         "status_test_util.h",
         "stl_util.h",
         "thread.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "distribution_helper_lib",
    srcs = [ "distribution_helper.cc" ],
    hdrs = [ "distribution_helper.h"  ],
    visibility = ["//visibility:public"],
    deps = [
         "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "md5_lib",
    srcs = [ "md5.cc" ],
    hdrs = [ "md5.h"  ],
    visibility = ["//visibility:public"],
    deps = [
         "//external:ssl_crypto",
    ],
)

cc_library(
    name = "simple_lru_cache",
    hdrs = [
         "simple_lru_cache_inl.h",
         "simple_lru_cache.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
         ":common_headers",
    ],
)

cc_library(
    name = "utils_lib",
    visibility = ["//visibility:public"],
    deps = [
         ":common_headers",
         ":distribution_helper_lib",
         ":md5_lib",
         ":simple_lru_cache",
    ],
)

cc_test(
    name = "distribution_helper_test",
    size = "small",
    srcs = [ "distribution_helper_test.cc" ],
    deps = [
        ":distribution_helper_lib",
        "//external:googletest_main",
    ],
)

cc_test(
    name = "md5_test",
    size = "small",
    srcs = [ "md5_test.cc" ],
    deps = [
         ":md5_lib",
         "//external:googletest_main",
    ],
)

cc_test(
    name = "simple_lru_cache_test",
    srcs = [ "simple_lru_cache_test.cc" ],
    size = "small",
    linkopts = [
         "-lm",
         "-lpthread",
    ],
    deps = [
        ":simple_lru_cache",
        "//external:googletest_main",
    ],
)
