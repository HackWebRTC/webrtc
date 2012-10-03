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
  void* aligned_buffer;
  void* memory_pointer;
};

uintptr_t GetRightAlign(uintptr_t start_pos, size_t alignment) {
  // The pointer should be aligned with |alignment| bytes. The - 1 guarantees
  // that it is aligned towards the closest higher (right) address.
  return (start_pos + alignment - 1) & ~(alignment - 1);
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
  uintptr_t start_pos = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void*>(GetRightAlign(start_pos, alignment));
}

void* AlignedMalloc(size_t size, size_t alignment) {
  if (size == 0) {
    return NULL;
  }
  if (!ValidAlignment(alignment)) {
    return NULL;
  }

  AlignedMemory* return_value = new AlignedMemory();
  if (return_value == NULL) {
    return NULL;
  }

  // The memory is aligned towards the lowest address that so only
  // alignment - 1 bytes needs to be allocated.
  // A pointer to AlignedMemory must be stored so that it can be retreived for
  // deletion, ergo the sizeof(uintptr_t).
  return_value->memory_pointer = malloc(size + sizeof(uintptr_t) +
                                        alignment - 1);
  if (return_value->memory_pointer == NULL) {
    delete return_value;
    return NULL;
  }

  // Aligning after the sizeof(header) bytes will leave room for the header
  // in the same memory block.
  uintptr_t align_start_pos =
      reinterpret_cast<uintptr_t>(return_value->memory_pointer);
  align_start_pos += sizeof(uintptr_t);
  uintptr_t aligned_pos = GetRightAlign(align_start_pos, alignment);
  return_value->aligned_buffer = reinterpret_cast<void*>(aligned_pos);

  // Store the address to the AlignedMemory struct in the header so that a
  // it's possible to reclaim all memory.
  uintptr_t header_pos = aligned_pos;
  header_pos -= sizeof(uintptr_t);
  void* header_ptr = reinterpret_cast<void*>(header_pos);
  uintptr_t header_value = reinterpret_cast<uintptr_t>(return_value);
  memcpy(header_ptr, &header_value, sizeof(uintptr_t));
  return return_value->aligned_buffer;
}

void AlignedFree(void* mem_block) {
  if (mem_block == NULL) {
    return;
  }
  uintptr_t aligned_pos = reinterpret_cast<uintptr_t>(mem_block);
  uintptr_t header_pos = aligned_pos - sizeof(uintptr_t);

  // Read out the address of the AlignedMemory struct from the header.
  uintptr_t* header_ptr = reinterpret_cast<uintptr_t*>(header_pos);
  AlignedMemory* delete_memory = reinterpret_cast<AlignedMemory*>(*header_ptr);

  if (delete_memory->memory_pointer != NULL) {
    free(delete_memory->memory_pointer);
  }
  delete delete_memory;
}

}  // namespace webrtc
