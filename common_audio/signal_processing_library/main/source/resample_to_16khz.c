/*
 * resample_to_16khz.c
 *
 * TODO(bjornv):
 *
 */

#include <string.h>
#include <stdlib.h>

#include "signal_processing_library.h"

/************************************************************
 *
 * WebRtcSpl_InitResamplerTo16(...)
 *
 * Initializes the mode of the resampler
 * allowed modes:
 *		8, 11, 12, 16, 22, 24, 32, 44, 48 (kHz)
 *
 * Returns	 0 - OK
 *			-1 - Error (unsupported mode)
 *
 ************************************************************/
WebRtc_Word16 WebRtcSpl_InitResamplerTo16(WebRtcSpl_StateTo16khz* state,
                                          WebRtc_Word16 mode)
{
    switch (mode)
    {
        case 8:
            state->blockSizeIn = 1;
            state->stepSizeIn = 1;
            state->blockSizeOut = 2;
            break;
        case 11:
            state->blockSizeIn = 18;
            state->stepSizeIn = 11;
            state->blockSizeOut = 8;
            break;
        case 12:
            state->blockSizeIn = 9;
            state->stepSizeIn = 3;
            state->blockSizeOut = 2;
            break;
        case 16:
            state->blockSizeIn = 1;
            state->stepSizeIn = 1;
            state->blockSizeOut = 1;
            break;
        case 22:
            state->blockSizeIn = 18;
            state->stepSizeIn = 11;
            state->blockSizeOut = 8;
            break;
        case 24:
            state->blockSizeIn = 9;
            state->stepSizeIn = 3;
            state->blockSizeOut = 2;
            break;
        case 32:
            state->blockSizeIn = 2;
            state->stepSizeIn = 2;
            state->blockSizeOut = 1;
            break;
        case 44:
            state->blockSizeIn = 18;
            state->stepSizeIn = 11;
            state->blockSizeOut = 8;
            break;
        case 48:
            state->blockSizeIn = 9;
            state->stepSizeIn = 3;
            state->blockSizeOut = 2;
            break;
        default:
            return -1;
    }

    state->mode = mode;
    WebRtcSpl_ResetResamplerTo16(state);
    return 0;
}

/************************************************************
 *
 * WebRtcSpl_ResetResamplerTo16(...)
 *
 * Resets the filter state of the resampler, but does not
 * change the mode
 *
 ************************************************************/
void WebRtcSpl_ResetResamplerTo16(WebRtcSpl_StateTo16khz* state)
{
    memset(state->upsampleBy2FilterState, 0, 8 * sizeof(WebRtc_Word32));
    memset(state->downsampleBy2FilterState, 0, 8 * sizeof(WebRtc_Word32));
    memset(state->speechBlockIn, 0, 18 * sizeof(WebRtc_Word32));
    memset(state->speechBlockIn, 0, 8 * sizeof(WebRtc_Word32));
    state->blockPositionIn = 0;
}

/***********************************************************
 *
 * Update the speechBlockIn buffer with new data
 * Internal function used by WebRtcSpl_ResamplerTo16()
 *
 ***********************************************************/
WebRtc_Word16 WebRtcSpl_BlockUpdateIn(WebRtcSpl_StateTo16khz *state, WebRtc_Word16 *data,
                              WebRtc_Word16 len, WebRtc_Word16 *pos)
{
    WebRtc_Word16 SamplesLeft = len - *pos;
    int i;

    if ((SamplesLeft + state->blockPositionIn) >= state->blockSizeIn)
    {
        for (i = 0; i < state->blockSizeIn - state->blockPositionIn; i++)
        {
            state->speechBlockIn[state->blockPositionIn + i] = (WebRtc_Word32)data[*pos];
            (*pos)++;
        }
        state->blockPositionIn = state->blockSizeIn;
        return 1;
    } else
    {
        for (i = 0; i < SamplesLeft; i++)
        {
            state->speechBlockIn[state->blockPositionIn + i] = (WebRtc_Word32)data[*pos];
            (*pos)++;
        }
        state->blockPositionIn += SamplesLeft;
        return 0;
    }
}

/***********************************************************
 *
 * Move data from speechBlockOut to data[] and update 
 * speechBlockIn buffer.
 * Internal function used by WebRtcSpl_ResamplerTo16()
 *
 ***********************************************************/

WebRtc_Word16 WebRtcSpl_BlockUpdateOut(WebRtcSpl_StateTo16khz *state, WebRtc_Word16 *data,
                               WebRtc_Word16 *pos)
{
    int i;
    for (i = 0; i < state->blockSizeOut; i++)
    {
        data[*pos]
                = (WebRtc_Word16)WEBRTC_SPL_SAT(32767,((state->speechBlockOut[i])>>15), -32768);
        (*pos)++;
    }
    /* Move data in input vector */
    state->blockPositionIn -= state->stepSizeIn;
    memmove(state->speechBlockIn, &(state->speechBlockIn[state->stepSizeIn]),
            sizeof(WebRtc_Word32) * (state->blockPositionIn));
    return 0;
}

/**********************************************************************************
 *
 * WebRtcSpl_ResamplerTo16(...)
 *
 * Resample input[] vector (with sample rate specified by init function) to 16 kHz 
 * and put result in output[] vector
 *
 * Limitation:
 *	For 32, 44 and 48 kHz input vector the number of input samples have to be even 
 *	if the output[] vectors given by WebRtcSpl_ResamplerTo16() are concatenated.
 *
 * Returns	 0 - OK
 *			-1 - Error (unsupported mode)
 *
 **********************************************************************************/
WebRtc_Word16 WebRtcSpl_ResamplerTo16(WebRtcSpl_StateTo16khz *state,
                                      WebRtc_Word16 *input, WebRtc_Word16 inlen,
                                      WebRtc_Word16 *output, WebRtc_Word16 *outlen)
{

    WebRtc_Word16 NoOfSamples;
    WebRtc_Word16 VecPosIn = 0;
    WebRtc_Word16 VecPosOut = 0;
    WebRtc_Word16 *tmpVec;

    switch (state->mode)
    {
        case 8:
            WebRtcSpl_UpsampleBy2(input, inlen, output, state->upsampleBy2FilterState);
            *outlen = inlen * 2;
            break;
        case 11:
            tmpVec = (WebRtc_Word16*)malloc(inlen * 2 * sizeof(WebRtc_Word16));
            WebRtcSpl_UpsampleBy2(input, inlen, tmpVec, state->upsampleBy2FilterState);
            NoOfSamples = inlen * 2;
            while (WebRtcSpl_BlockUpdateIn(state, tmpVec, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample44khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            free(tmpVec);
            break;
        case 12:
            tmpVec = (WebRtc_Word16*)malloc(inlen * 2 * sizeof(WebRtc_Word16));
            WebRtcSpl_UpsampleBy2(input, inlen, tmpVec, state->upsampleBy2FilterState);
            NoOfSamples = inlen * 2;
            while (WebRtcSpl_BlockUpdateIn(state, tmpVec, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample48khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            free(tmpVec);
            break;
        case 16:
            memcpy(output, input, inlen * sizeof(WebRtc_Word16));
            *outlen = inlen;
            break;
        case 22:
            NoOfSamples = inlen;
            while (WebRtcSpl_BlockUpdateIn(state, input, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample44khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            break;
        case 24:
            NoOfSamples = inlen;
            while (WebRtcSpl_BlockUpdateIn(state, input, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample48khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            break;
        case 32:
            WebRtcSpl_DownsampleBy2(input, inlen, output, state->downsampleBy2FilterState);
            *outlen = inlen >> 1;
            break;
        case 44:
            tmpVec = (WebRtc_Word16*)malloc((inlen >> 1) * sizeof(WebRtc_Word16));
            WebRtcSpl_DownsampleBy2(input, inlen, tmpVec, state->downsampleBy2FilterState);
            NoOfSamples = inlen >> 1;
            while (WebRtcSpl_BlockUpdateIn(state, tmpVec, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample44khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            free(tmpVec);
            break;
        case 48:
            tmpVec = (WebRtc_Word16*)malloc((inlen >> 1) * sizeof(WebRtc_Word16));
            WebRtcSpl_DownsampleBy2(input, inlen, tmpVec, state->downsampleBy2FilterState);
            NoOfSamples = inlen >> 1;
            while (WebRtcSpl_BlockUpdateIn(state, tmpVec, NoOfSamples, &VecPosIn))
            {
                WebRtcSpl_Resample48khzTo32khz(state->speechBlockIn, state->speechBlockOut, 1);
                WebRtcSpl_BlockUpdateOut(state, output, &VecPosOut);
            }
            *outlen = VecPosOut;
            free(tmpVec);
            break;
        default:
            return -1;
    }
    return 0;

}
