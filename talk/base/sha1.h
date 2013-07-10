/*
 * SHA-1 in C
 * By Steve Reid <sreid@sea-to-sky.net>
 * 100% Public Domain
 *
*/

// Ported to C++, Google style and uses basictypes.h

#ifndef TALK_BASE_SHA1_H_
#define TALK_BASE_SHA1_H_

#include "talk/base/basictypes.h"

struct SHA1_CTX {
  uint32 state[5];
  // TODO: Change bit count to uint64.
  uint32 count[2];  // Bit count of input.
  uint8 buffer[64];
};

#define SHA1_DIGEST_SIZE 20

void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const uint8* data, size_t len);
void SHA1Final(SHA1_CTX* context, uint8 digest[SHA1_DIGEST_SIZE]);

#endif  // TALK_BASE_SHA1_H_
