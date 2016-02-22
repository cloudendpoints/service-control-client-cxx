
cc_library(
    name = "common_headers",
    hdrs = [
      "google_macros.h",
      "status_test_util.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "distribution_helper_lib",
    srcs = [ "distribution_helper.cc" ],
    hdrs = [ "distribution_helper.h" ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/config:servicecontrol",
    ],
)

cc_library(
    name = "money_utils_lib",
    srcs = [ "money_utils.cc" ],
    hdrs = [ "money_utils.h" ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/config:servicecontrol",
    ],
)

cc_test(
    name = "money_utils_test",
    size = "small",
    srcs = [ "money_utils_test.cc" ],
    deps = [
        ":common_headers",
        ":money_utils_lib",
        "//external:googletest_main",
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
    name = "service_control_proto_test",
    size = "small",
    srcs = [
        "service_control_proto_test.cc",
    ],
    deps = [
        "//external:googletest_main",
        "//third_party/config:servicecontrol",
    ],
)
