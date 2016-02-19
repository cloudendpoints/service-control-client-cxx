#ifndef GOOGLE_SERVICE_CONTROL_CLIENT_MD5_H_
#define GOOGLE_SERVICE_CONTROL_CLIENT_MD5_H_

#include <string>
#include "openssl/md5.h"

namespace google {
namespace service_control_client {

// Define a MD5 Digest by calling OpenSSL
class MD5 {
 public:
  MD5();

  MD5& Update(const void* data, size_t size);

  // The MD5 digest is always 128 bits = 16 bytes
  static const int kDigestLength = 16;

  std::string Digest();

  // A short form of generating MD5 for a string
  std::string operator()(const void* data, size_t size);

  // For debugging and unit-test only.
  std::string PrintableDigest();

 private:
  MD5_CTX ctx_;
  unsigned char digest_[kDigestLength];
  bool finalized_;
};

}  // namespace service_control_client
}  // namespace google

#endif  // GOOGLE_SERVICE_CONTROL_CLIENT_MD5_H_
