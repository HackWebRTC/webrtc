/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Specifies the interface for the AEC generic buffer.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_

// Determines buffer datatype
typedef short bufdata_t;

// Unless otherwise specified, functions return 0 on success and -1 on error
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

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_RING_BUFFER_H_
