# The Service Control Client library for c/c++ #

The service control client library provides C++ APIs for:

* access control check on API key validation
* reporting API related data for google cloud logging and google cloud
  monitoring

It offers:

* fast access control check through caching
* high scalability by significantly reducing outgoing check and report requests
  through aggregation

[TOC]


## Getting Service Control Client library ##

To download the service control client source code, clone the repository:

    # Clone the repository
    git clone sso://gcp-apis/service-control-client-cxx

## Repository Structure ##

* [include](/include): The folder contains public headers.
* [utils](/utils): The folder contains utility code.
* [src](/src): The folder contains core source code.

## Setup, Build and Test ##

Recommended workflow to setup, build and test service control client code:

    # Sync your git repository with the origin.
    git checkout master
    git pull origin --rebase

    # Setup git for remote push
    script/setup

    # Use Bazel to build
    bazel build :all

    # Use Bazel to test
    bazel test :all
