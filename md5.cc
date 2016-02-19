#include "md5.h"
#include <assert.h>

namespace google {
namespace service_control_client {

MD5::MD5() : finalized_(false) { MD5_Init(&ctx_); }

MD5& MD5::Update(const void* data, size_t size) {
  // Not update after finalized.
  assert(!finalized_);
  MD5_Update(&ctx_, data, size);
  return *this;
}

std::string MD5::Digest() {
  if (!finalized_) {
    MD5_Final(digest_, &ctx_);
    finalized_ = true;
  }
  return std::string(reinterpret_cast<char*>(digest_), kDigestLength);
}

std::string MD5::PrintableDigest() {
  if (!finalized_) {
    MD5_Final(digest_, &ctx_);
    finalized_ = true;
  }
  char buf[kDigestLength * 2 + 1];
  char* p = buf;
  for (int i = 0; i < kDigestLength; i++, p += 2) {
    sprintf(p, "%02x", digest_[i]);
  }
  *p = 0;
  return std::string(buf, kDigestLength * 2);
}

std::string MD5::operator()(const void* data, size_t size) {
  return Update(data, size).Digest();
}

}  // namespace service_control_client
}  // namespace google
