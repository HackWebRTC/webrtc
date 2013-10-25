/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/helpers.h"

#include <limits>

#include "talk/base/sslconfig.h"
#if defined(SSL_USE_OPENSSL)
#include <openssl/rand.h>
#elif defined(SSL_USE_NSS_RNG)
#include "pk11func.h"
#else
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ntsecapi.h>
#endif  // WIN32
#endif

#include "talk/base/base64.h"
#include "talk/base/basictypes.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/timeutils.h"

// Protect against max macro inclusion.
#undef max

namespace talk_base {

// Base class for RNG implementations.
class RandomGenerator {
 public:
  virtual ~RandomGenerator() {}
  virtual bool Init(const void* seed, size_t len) = 0;
  virtual bool Generate(void* buf, size_t len) = 0;
};

#if defined(SSL_USE_OPENSSL)
// The OpenSSL RNG. Need to make sure it doesn't run out of entropy.
class SecureRandomGenerator : public RandomGenerator {
 public:
  SecureRandomGenerator() : inited_(false) {
  }
  ~SecureRandomGenerator() {
  }
  virtual bool Init(const void* seed, size_t len) {
    // By default, seed from the system state.
    if (!inited_) {
      if (RAND_poll() <= 0) {
        return false;
      }
      inited_ = true;
    }
    // Allow app data to be mixed in, if provided.
    if (seed) {
      RAND_seed(seed, len);
    }
    return true;
  }
  virtual bool Generate(void* buf, size_t len) {
    if (!inited_ && !Init(NULL, 0)) {
      return false;
    }
    return (RAND_bytes(reinterpret_cast<unsigned char*>(buf), len) > 0);
  }

 private:
  bool inited_;
};

#elif defined(SSL_USE_NSS_RNG)
// The NSS RNG.
class SecureRandomGenerator : public RandomGenerator {
 public:
  SecureRandomGenerator() {}
  ~SecureRandomGenerator() {}
  virtual bool Init(const void* seed, size_t len) {
    return true;
  }
  virtual bool Generate(void* buf, size_t len) {
    return (PK11_GenerateRandom(reinterpret_cast<unsigned char*>(buf),
                                static_cast<int>(len)) == SECSuccess);
  }
};

#else
#ifdef WIN32
class SecureRandomGenerator : public RandomGenerator {
 public:
  SecureRandomGenerator() : advapi32_(NULL), rtl_gen_random_(NULL) {}
  ~SecureRandomGenerator() {
    FreeLibrary(advapi32_);
  }

  virtual bool Init(const void* seed, size_t seed_len) {
    // We don't do any additional seeding on Win32, we just use the CryptoAPI
    // RNG (which is exposed as a hidden function off of ADVAPI32 so that we
    // don't need to drag in all of CryptoAPI)
    if (rtl_gen_random_) {
      return true;
    }

    advapi32_ = LoadLibrary(L"advapi32.dll");
    if (!advapi32_) {
      return false;
    }

    rtl_gen_random_ = reinterpret_cast<RtlGenRandomProc>(
        GetProcAddress(advapi32_, "SystemFunction036"));
    if (!rtl_gen_random_) {
      FreeLibrary(advapi32_);
      return false;
    }

    return true;
  }
  virtual bool Generate(void* buf, size_t len) {
    if (!rtl_gen_random_ && !Init(NULL, 0)) {
      return false;
    }
    return (rtl_gen_random_(buf, static_cast<int>(len)) != FALSE);
  }

 private:
  typedef BOOL (WINAPI *RtlGenRandomProc)(PVOID, ULONG);
  HINSTANCE advapi32_;
  RtlGenRandomProc rtl_gen_random_;
};

#else

#error No SSL implementation has been selected!

#endif  // WIN32
#endif

// A test random generator, for predictable output.
class TestRandomGenerator : public RandomGenerator {
 public:
  TestRandomGenerator() : seed_(7) {
  }
  ~TestRandomGenerator() {
  }
  virtual bool Init(const void* seed, size_t len) {
    return true;
  }
  virtual bool Generate(void* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      static_cast<uint8*>(buf)[i] = static_cast<uint8>(GetRandom());
    }
    return true;
  }

 private:
  int GetRandom() {
    return ((seed_ = seed_ * 214013L + 2531011L) >> 16) & 0x7fff;
  }
  int seed_;
};

// TODO: Use Base64::Base64Table instead.
static const char BASE64[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

namespace {

// This round about way of creating a global RNG is to safe-guard against
// indeterminant static initialization order.
scoped_ptr<RandomGenerator>& GetGlobalRng() {
  LIBJINGLE_DEFINE_STATIC_LOCAL(scoped_ptr<RandomGenerator>, global_rng,
                                (new SecureRandomGenerator()));
  return global_rng;
}

RandomGenerator& Rng() {
  return *GetGlobalRng();
}

}  // namespace

void SetRandomTestMode(bool test) {
  if (!test) {
    GetGlobalRng().reset(new SecureRandomGenerator());
  } else {
    GetGlobalRng().reset(new TestRandomGenerator());
  }
}

bool InitRandom(int seed) {
  return InitRandom(reinterpret_cast<const char*>(&seed), sizeof(seed));
}

bool InitRandom(const char* seed, size_t len) {
  if (!Rng().Init(seed, len)) {
    LOG(LS_ERROR) << "Failed to init random generator!";
    return false;
  }
  return true;
}

std::string CreateRandomString(size_t len) {
  std::string str;
  CreateRandomString(len, &str);
  return str;
}

bool CreateRandomString(size_t len,
                        const char* table, int table_size,
                        std::string* str) {
  str->clear();
  scoped_ptr<uint8[]> bytes(new uint8[len]);
  if (!Rng().Generate(bytes.get(), len)) {
    LOG(LS_ERROR) << "Failed to generate random string!";
    return false;
  }
  str->reserve(len);
  for (size_t i = 0; i < len; ++i) {
    str->push_back(table[bytes[i] % table_size]);
  }
  return true;
}

bool CreateRandomString(size_t len, std::string* str) {
  return CreateRandomString(len, BASE64, 64, str);
}

bool CreateRandomString(size_t len, const std::string& table,
                        std::string* str) {
  return CreateRandomString(len, table.c_str(),
                            static_cast<int>(table.size()), str);
}

uint32 CreateRandomId() {
  uint32 id;
  if (!Rng().Generate(&id, sizeof(id))) {
    LOG(LS_ERROR) << "Failed to generate random id!";
  }
  return id;
}

uint64 CreateRandomId64() {
  return static_cast<uint64>(CreateRandomId()) << 32 | CreateRandomId();
}

uint32 CreateRandomNonZeroId() {
  uint32 id;
  do {
    id = CreateRandomId();
  } while (id == 0);
  return id;
}

double CreateRandomDouble() {
  return CreateRandomId() / (std::numeric_limits<uint32>::max() +
      std::numeric_limits<double>::epsilon());
}

}  // namespace talk_base
