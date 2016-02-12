# A Bazel (http://bazel.io) workspace for the Google Service Control client

new_git_repository(
    name = "googletest_git",
    build_file = "BUILD.googletest",
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
