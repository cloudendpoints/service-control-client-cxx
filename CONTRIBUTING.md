# How to contribute

## Contributor License Agreements

We'd love to accept your patches! Before we can take them, we have to jump a
couple of legal hurdles.

Please fill out either the individual or corporate Contributor License Agreement
(CLA).

* If you are an individual writing original source code and you're sure you
  own the intellectual property, then you'll need to sign an
  [individual CLA](http://code.google.com/legal/individual-cla-v1.0.html).

* If you work for a company that wants to allow you to contribute your work,
  then you'll need to sign a
  [corporate CLA](http://code.google.com/legal/corporate-cla-v1.0.html).

Follow either of the two links above to access the appropriate CLA and
instructions for how to sign and return it. Once we receive it, we'll be able
to accept your pull requests.

***NOTE***: Only original source code from you and other people that have signed
the CLA can be accepted into the main repository. This policy does not apply to
[third_party](third_party/).

## Contributing A Patch

1. Submit an issue describing your proposed change to the repo in question.
2. The repo owner will respond to your issue promptly.
3. If your proposed change is accepted, and you haven't already done so, sign a
   Contributor License Agreement (see details above).
4. Fork the desired repo, develop and test your code changes.
5. Submit a pull request.

## Include Tests & Documentation

If you change or add functionality, your changes should include the necessary
tests to prove that it works. While working on local code changes, always run
the tests. Any change that could affect a user's experience also needs a change
or addition to the relevant documentation.

Pull requests that do not include sufficient tests or documentation will be
rejected.

## Testing your changes

    bazel test :all
