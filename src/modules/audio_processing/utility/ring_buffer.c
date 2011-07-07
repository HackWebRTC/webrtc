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
 * Provides a generic ring buffer that can be written to and read from with
 * arbitrarily sized blocks. The AEC uses this for several different tasks.
 */

#include <stdlib.h>
#include <string.h>
#include "ring_buffer.h"

typedef struct {
    int readPos;
    int writePos;
    int size;
    char rwWrap;
    bufdata_t *data;
} buf_t;

enum {SAME_WRAP, DIFF_WRAP};

int WebRtcApm_CreateBuffer(void **bufInst, int size)
{
    buf_t *buf = NULL;

    if (size < 0) {
        return -1;
    }

    buf = malloc(sizeof(buf_t));
    *bufInst = buf;
    if (buf == NULL) {
        return -1;
    }

    buf->data = malloc(size*sizeof(bufdata_t));
    if (buf->data == NULL) {
        free(buf);
        buf = NULL;
        return -1;
    }

    buf->size = size;
    return 0;
}

int WebRtcApm_InitBuffer(void *bufInst)
{
    buf_t *buf = (buf_t*)bufInst;

    buf->readPos = 0;
    buf->writePos = 0;
    buf->rwWrap = SAME_WRAP;

    // Initialize buffer to zeros
    memset(buf->data, 0, sizeof(bufdata_t)*buf->size);

    return 0;
}

int WebRtcApm_FreeBuffer(void *bufInst)
{
    buf_t *buf = (buf_t*)bufInst;

    if (buf == NULL) {
        return -1;
    }

    free(buf->data);
    free(buf);

    return 0;
}

int WebRtcApm_ReadBuffer(void *bufInst, bufdata_t *data, int size)
{
    buf_t *buf = (buf_t*)bufInst;
    int n = 0, margin = 0;

    if (size <= 0 || size > buf->size) {
        return -1;
    }

    n = size;
    if (buf->rwWrap == DIFF_WRAP) {
        margin = buf->size - buf->readPos;
        if (n > margin) {
            buf->rwWrap = SAME_WRAP;
            memcpy(data, buf->data + buf->readPos,
                sizeof(bufdata_t)*margin);
            buf->readPos = 0;
            n = size - margin;
        }
        else {
            memcpy(data, buf->data + buf->readPos,
                sizeof(bufdata_t)*n);
            buf->readPos += n;
            return n;
        }
    }

    if (buf->rwWrap == SAME_WRAP) {
        margin = buf->writePos - buf->readPos;
        if (margin > n)
            margin = n;
        memcpy(data + size - n, buf->data + buf->readPos,
            sizeof(bufdata_t)*margin);
        buf->readPos += margin;
        n -= margin;
    }

    return size - n;
}

int WebRtcApm_WriteBuffer(void *bufInst, const bufdata_t *data, int size)
{
    buf_t *buf = (buf_t*)bufInst;
    int n = 0, margin = 0;

    if (size < 0 || size > buf->size) {
        return -1;
    }

    n = size;
    if (buf->rwWrap == SAME_WRAP) {
        margin = buf->size - buf->writePos;
        if (n > margin) {
            buf->rwWrap = DIFF_WRAP;
            memcpy(buf->data + buf->writePos, data,
                sizeof(bufdata_t)*margin);
            buf->writePos = 0;
            n = size - margin;
        }
        else {
            memcpy(buf->data + buf->writePos, data,
                sizeof(bufdata_t)*n);
            buf->writePos += n;
            return n;
        }
    }

    if (buf->rwWrap == DIFF_WRAP) {
        margin = buf->readPos - buf->writePos;
        if (margin > n)
            margin = n;
        memcpy(buf->data + buf->writePos, data + size - n,
            sizeof(bufdata_t)*margin);
        buf->writePos += margin;
        n -= margin;
    }

    return size - n;
}

int WebRtcApm_FlushBuffer(void *bufInst, int size)
{
    buf_t *buf = (buf_t*)bufInst;
    int n = 0, margin = 0;

    if (size <= 0 || size > buf->size) {
        return -1;
    }

    n = size;
    if (buf->rwWrap == DIFF_WRAP) {
        margin = buf->size - buf->readPos;
        if (n > margin) {
            buf->rwWrap = SAME_WRAP;
            buf->readPos = 0;
            n = size - margin;
        }
        else {
            buf->readPos += n;
            return n;
        }
    }

    if (buf->rwWrap == SAME_WRAP) {
        margin = buf->writePos - buf->readPos;
        if (margin > n)
            margin = n;
        buf->readPos += margin;
        n -= margin;
    }

    return size - n;
}

int WebRtcApm_StuffBuffer(void *bufInst, int size)
{
    buf_t *buf = (buf_t*)bufInst;
    int n = 0, margin = 0;

    if (size <= 0 || size > buf->size) {
        return -1;
    }

    n = size;
    if (buf->rwWrap == SAME_WRAP) {
        margin = buf->readPos;
        if (n > margin) {
            buf->rwWrap = DIFF_WRAP;
            buf->readPos = buf->size - 1;
            n -= margin + 1;
        }
        else {
            buf->readPos -= n;
            return n;
        }
    }

    if (buf->rwWrap == DIFF_WRAP) {
        margin = buf->readPos - buf->writePos;
        if (margin > n)
            margin = n;
        buf->readPos -= margin;
        n -= margin;
    }

    return size - n;
}

int WebRtcApm_get_buffer_size(const void *bufInst)
{
    const buf_t *buf = (buf_t*)bufInst;

    if (buf->rwWrap == SAME_WRAP)
        return buf->writePos - buf->readPos;
    else
        return buf->size - buf->readPos + buf->writePos;
}
