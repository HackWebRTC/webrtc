/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aligned_malloc.h"

#include <memory.h>
#include <stdlib.h>

#if _WIN32
#include <windows.h>
#else
#include <stdint.h>
#endif

#include "typedefs.h"

// Reference on memory alignment:
// http://stackoverflow.com/questions/227897/solve-the-memory-alignment-in-c-interview-question-that-stumped-me
namespace webrtc {
// TODO(henrike): better to create just one memory block and interpret the
//                first sizeof(AlignedMemory) bytes as an AlignedMemory struct.
struct AlignedMemory {
  void* alignedBuffer;
  void* memoryPointer;
};

uintptr_t GetRightAlign(uintptr_t startPos, size_t alignment) {
  // The pointer should be aligned with |alignment| bytes. The - 1 guarantees
  // that it is aligned towards the closest higher (right) address.
  return (startPos + alignment - 1) & ~(alignment - 1);
}

// Alignment must be an integer power of two.
bool ValidAlignment(size_t alignment) {
  if (!alignment) {
    return false;
  }
  return (alignment & (alignment - 1)) == 0;
}

void* GetRightAlign(const void* ptr, size_t alignment) {
  if (!ptr) {
    return NULL;
  }
  if (!ValidAlignment(alignment)) {
    return NULL;
  }
  uintptr_t startPos = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void*>(GetRightAlign(startPos, alignment));
}

void* AlignedMalloc(size_t size, size_t alignment) {
  if (size == 0) {
    return NULL;
  }
  if (!ValidAlignment(alignment)) {
    return NULL;
  }

  AlignedMemory* returnValue = new AlignedMemory();
  if (returnValue == NULL) {
    return NULL;
  }

  // The memory is aligned towards the lowest address that so only
  // alignment - 1 bytes needs to be allocated.
  // A pointer to AlignedMemory must be stored so that it can be retreived for
  // deletion, ergo the sizeof(uintptr_t).
  returnValue->memoryPointer = malloc(size + sizeof(uintptr_t) +
                                      alignment - 1);
  if (returnValue->memoryPointer == NULL) {
    delete returnValue;
    return NULL;
  }

  // Aligning after the sizeof(header) bytes will leave room for the header
  // in the same memory block.
  uintptr_t alignStartPos =
      reinterpret_cast<uintptr_t>(returnValue->memoryPointer);
  alignStartPos += sizeof(uintptr_t);
  uintptr_t alignedPos = GetRightAlign(alignStartPos, alignment);
  returnValue->alignedBuffer = reinterpret_cast<void*>(alignedPos);

  // Store the address to the AlignedMemory struct in the header so that a
  // it's possible to reclaim all memory.
  uintptr_t headerPos = alignedPos;
  headerPos -= sizeof(uintptr_t);
  void* headerPtr = reinterpret_cast<void*>(headerPos);
  uintptr_t headerValue = reinterpret_cast<uintptr_t>(returnValue);
  memcpy(headerPtr,&headerValue,sizeof(uintptr_t));
  return returnValue->alignedBuffer;
}

void AlignedFree(void* memBlock) {
  if (memBlock == NULL) {
    return;
  }
  uintptr_t alignedPos = reinterpret_cast<uintptr_t>(memBlock);
  uintptr_t headerPos = alignedPos - sizeof(uintptr_t);

  // Read out the address of the AlignedMemory struct from the header.
  uintptr_t* headerPtr = reinterpret_cast<uintptr_t*>(headerPos);
  AlignedMemory* deleteMemory = reinterpret_cast<AlignedMemory*>(*headerPtr);

  if (deleteMemory->memoryPointer != NULL) {
    free(deleteMemory->memoryPointer);
  }
  delete deleteMemory;
}

}  // namespace webrtc
