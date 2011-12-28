/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A ring buffer to hold arbitrary data. Provides no thread safety. Unless
// otherwise specified, functions return 0 on success and -1 on error.

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_

#include <stddef.h> // size_t

// Determines buffer datatype
typedef short bufdata_t; // TODO(bjornv): Remove together with the below.

// TODO(bjornv): Remove WebRtcApm_CreateBuffer, WebRtcApm_InitBuffer and
//               WebRtcApm_FreeBuffer when replaced in APM.
//               Rename WebRtcApm_FreeBuffer to WebRtc_FreeBuffer() and replace.
//               Replace WebRtcApm_FlushBuffer and WebRtcApm_StuffBuffer with
//               WebRtc_MoveReadPtr and Read/Write-Buffer with its new versions.

int WebRtcApm_CreateBuffer(void **bufInst, int size);
int WebRtcApm_InitBuffer(void *bufInst);
int WebRtcApm_FreeBuffer(void *bufInst);

// Returns number of samples read
int WebRtcApm_ReadBuffer(void *bufInst, bufdata_t *data, int size);

// Returns number of samples written
int WebRtcApm_WriteBuffer(void *bufInst, const bufdata_t *data, int size);

// Returns number of samples flushed
int WebRtcApm_FlushBuffer(void *bufInst, int size);

// Returns number of samples stuffed
int WebRtcApm_StuffBuffer(void *bufInst, int size);

// Returns number of samples in buffer
int WebRtcApm_get_buffer_size(const void *bufInst);

// TODO(bjornv): Below are the new functions, to replace the older ones above.
int WebRtc_CreateBuffer(void** handle,
                        size_t element_count,
                        size_t element_size);
int WebRtc_InitBuffer(void* handle);
int WebRtc_FreeBuffer(void* handle);

// Reads data from the buffer. The |data_ptr| will point to the address where
// it is located. If all |element_count| data are feasible to read without
// buffer wrap around |data_ptr| will point to the location in the buffer.
// Otherwise, the data will be copied to |data| (memory allocation done by the
// user) and |data_ptr| points to the address of |data|. |data_ptr| is only
// guaranteed to be valid until the next call to WebRtc_WriteBuffer().
// Returns number of elements read.
size_t WebRtc_ReadBuffer(void* handle,
                         void** data_ptr,
                         void* data,
                         size_t element_count);

// Writes |data| to buffer and returns the number of elements written.
size_t WebRtc_WriteBuffer(void* handle, const void* data, size_t element_count);

// Moves the buffer read position and returns the number of elements moved.
// Positive |element_count| moves the read position towards the write position,
// that is, flushing the buffer. Negative |element_count| moves the read
// position away from the the write position, that is, stuffing the buffer.
// Returns number of elements moved.
int WebRtc_MoveReadPtr(void* handle, int element_count);

// Returns number of available elements to read.
size_t WebRtc_available_read(const void* handle);

// Returns number of available elements for write.
size_t WebRtc_available_write(const void* handle);

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_
