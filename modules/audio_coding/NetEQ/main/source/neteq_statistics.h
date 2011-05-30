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
 * Definitions of statistics data structures for MCU and DSP sides.
 */

#include "typedefs.h"

#ifndef NETEQ_STATISTICS_H
#define NETEQ_STATISTICS_H

/*
 * Statistics struct on DSP side
 */
typedef struct
{

    /* variables for in-call statistics; queried through WebRtcNetEQ_GetNetworkStatistics */
    WebRtc_UWord32 expandLength; /* number of samples produced through expand */
    WebRtc_UWord32 preemptiveLength; /* number of samples produced through pre-emptive
     expand */
    WebRtc_UWord32 accelerateLength; /* number of samples removed through accelerate */

    /* variables for post-call statistics; queried through WebRtcNetEQ_GetJitterStatistics */
    WebRtc_UWord32 expandedVoiceSamples; /* number of voice samples produced through expand */
    WebRtc_UWord32 expandedNoiseSamples; /* number of noise (background) samples produced
     through expand */

} DSPStats_t;

/*
 * Statistics struct on MCU side
 * All variables are for post-call statistics; queried through WebRtcNetEQ_GetJitterStatistics.
 */
typedef struct
{

    WebRtc_UWord32 jbMinSize; /* smallest Jitter Buffer size during call in ms */
    WebRtc_UWord32 jbMaxSize; /* largest Jitter Buffer size during call in ms */
    WebRtc_UWord32 jbAvgSizeQ16; /* the average JB size, measured over time in ms (Q16) */
    WebRtc_UWord16 jbAvgCount; /* help counter for jbAveSize */
    WebRtc_UWord32 minPacketDelayMs; /* min time incoming packet "waited" to be played in ms */
    WebRtc_UWord32 maxPacketDelayMs; /* max time incoming packet "waited" to be played in ms */
    WebRtc_UWord16 avgPacketDelayMs; /* avg time incoming packet "waited" to be played in ms */
    WebRtc_UWord16 avgPacketCount; /* help counter for avgPacketDelayMs */
    WebRtc_UWord32 jbChangeCount; /* count number of successful accelerate and pre-emptive
     expand operations */
    WebRtc_UWord32 generatedSilentMs; /* generated silence in ms */
    WebRtc_UWord32 countExpandMoreThan120ms; /* count of tiny expansions */
    WebRtc_UWord32 countExpandMoreThan250ms; /* count of small expansions */
    WebRtc_UWord32 countExpandMoreThan500ms; /* count of medium expansions */
    WebRtc_UWord32 countExpandMoreThan2000ms; /* count of large expansions */
    WebRtc_UWord32 longestExpandDurationMs; /* duration of longest expansions in ms */
    WebRtc_UWord32 accelerateMs; /* audio data removed through accelerate in ms */

} MCUStats_t;

#endif

