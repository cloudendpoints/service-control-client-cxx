
cc_library(
    name = "simple_lru_cache",
    hdrs = [
        "simple_lru_cache_inl.h",
        "simple_lru_cache.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:common_headers",
    ],
)

cc_test(
    name = "simple_lru_cache_test",
    srcs = [
        "simple_lru_cache_test.cc",
    ],
    size = "small",
    linkopts = [
      "-static",
      "-lstdc++",
      "-lm",
      "-ldl",
      "-lpthread",
    ],
    deps = [
        ":simple_lru_cache",
        "//external:googletest_main",
    ],
)
