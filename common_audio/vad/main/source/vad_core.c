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
 * This file includes the implementation of the core functionality in VAD.
 * For function description, see vad_core.h.
 */

#include "vad_core.h"
#include "vad_const.h"
#include "vad_defines.h"
#include "vad_filterbank.h"
#include "vad_gmm.h"
#include "vad_sp.h"
#include "signal_processing_library.h"

static const int kInitCheck = 42;

// Initialize VAD
int WebRtcVad_InitCore(VadInstT *inst, short mode)
{
    int i;

    // Initialization of struct
    inst->vad = 1;
    inst->frame_counter = 0;
    inst->over_hang = 0;
    inst->num_of_speech = 0;

    // Initialization of downsampling filter state
    inst->downsampling_filter_states[0] = 0;
    inst->downsampling_filter_states[1] = 0;
    inst->downsampling_filter_states[2] = 0;
    inst->downsampling_filter_states[3] = 0;

    // Read initial PDF parameters
    for (i = 0; i < NUM_TABLE_VALUES; i++)
    {
        inst->noise_means[i] = kNoiseDataMeans[i];
        inst->speech_means[i] = kSpeechDataMeans[i];
        inst->noise_stds[i] = kNoiseDataStds[i];
        inst->speech_stds[i] = kSpeechDataStds[i];
    }

    // Index and Minimum value vectors are initialized
    for (i = 0; i < 16 * NUM_CHANNELS; i++)
    {
        inst->low_value_vector[i] = 10000;
        inst->index_vector[i] = 0;
    }

    for (i = 0; i < 5; i++)
    {
        inst->upper_state[i] = 0;
        inst->lower_state[i] = 0;
    }

    for (i = 0; i < 4; i++)
    {
        inst->hp_filter_state[i] = 0;
    }

    // Init mean value memory, for FindMin function
    inst->mean_value[0] = 1600;
    inst->mean_value[1] = 1600;
    inst->mean_value[2] = 1600;
    inst->mean_value[3] = 1600;
    inst->mean_value[4] = 1600;
    inst->mean_value[5] = 1600;

    if (mode == 0)
    {
        // Quality mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_Q; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_Q; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_Q; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_Q; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_Q; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_Q; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_Q;
        inst->individual[1] = INDIVIDUAL_20MS_Q;
        inst->individual[2] = INDIVIDUAL_30MS_Q;

        inst->total[0] = TOTAL_10MS_Q;
        inst->total[1] = TOTAL_20MS_Q;
        inst->total[2] = TOTAL_30MS_Q;
    } else if (mode == 1)
    {
        // Low bitrate mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_LBR; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_LBR; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_LBR; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_LBR; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_LBR; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_LBR; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_LBR;
        inst->individual[1] = INDIVIDUAL_20MS_LBR;
        inst->individual[2] = INDIVIDUAL_30MS_LBR;

        inst->total[0] = TOTAL_10MS_LBR;
        inst->total[1] = TOTAL_20MS_LBR;
        inst->total[2] = TOTAL_30MS_LBR;
    } else if (mode == 2)
    {
        // Aggressive mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_AGG; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_AGG; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_AGG; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_AGG; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_AGG; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_AGG; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_AGG;
        inst->individual[1] = INDIVIDUAL_20MS_AGG;
        inst->individual[2] = INDIVIDUAL_30MS_AGG;

        inst->total[0] = TOTAL_10MS_AGG;
        inst->total[1] = TOTAL_20MS_AGG;
        inst->total[2] = TOTAL_30MS_AGG;
    } else
    {
        // Very aggressive mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_VAG; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_VAG; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_VAG; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_VAG; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_VAG; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_VAG; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_VAG;
        inst->individual[1] = INDIVIDUAL_20MS_VAG;
        inst->individual[2] = INDIVIDUAL_30MS_VAG;

        inst->total[0] = TOTAL_10MS_VAG;
        inst->total[1] = TOTAL_20MS_VAG;
        inst->total[2] = TOTAL_30MS_VAG;
    }

    inst->init_flag = kInitCheck;

    return 0;
}

// Set aggressiveness mode
int WebRtcVad_set_mode_core(VadInstT *inst, short mode)
{

    if (mode == 0)
    {
        // Quality mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_Q; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_Q; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_Q; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_Q; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_Q; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_Q; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_Q;
        inst->individual[1] = INDIVIDUAL_20MS_Q;
        inst->individual[2] = INDIVIDUAL_30MS_Q;

        inst->total[0] = TOTAL_10MS_Q;
        inst->total[1] = TOTAL_20MS_Q;
        inst->total[2] = TOTAL_30MS_Q;
    } else if (mode == 1)
    {
        // Low bitrate mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_LBR; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_LBR; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_LBR; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_LBR; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_LBR; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_LBR; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_LBR;
        inst->individual[1] = INDIVIDUAL_20MS_LBR;
        inst->individual[2] = INDIVIDUAL_30MS_LBR;

        inst->total[0] = TOTAL_10MS_LBR;
        inst->total[1] = TOTAL_20MS_LBR;
        inst->total[2] = TOTAL_30MS_LBR;
    } else if (mode == 2)
    {
        // Aggressive mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_AGG; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_AGG; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_AGG; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_AGG; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_AGG; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_AGG; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_AGG;
        inst->individual[1] = INDIVIDUAL_20MS_AGG;
        inst->individual[2] = INDIVIDUAL_30MS_AGG;

        inst->total[0] = TOTAL_10MS_AGG;
        inst->total[1] = TOTAL_20MS_AGG;
        inst->total[2] = TOTAL_30MS_AGG;
    } else if (mode == 3)
    {
        // Very aggressive mode
        inst->over_hang_max_1[0] = OHMAX1_10MS_VAG; // Overhang short speech burst
        inst->over_hang_max_1[1] = OHMAX1_20MS_VAG; // Overhang short speech burst
        inst->over_hang_max_1[2] = OHMAX1_30MS_VAG; // Overhang short speech burst
        inst->over_hang_max_2[0] = OHMAX2_10MS_VAG; // Overhang long speech burst
        inst->over_hang_max_2[1] = OHMAX2_20MS_VAG; // Overhang long speech burst
        inst->over_hang_max_2[2] = OHMAX2_30MS_VAG; // Overhang long speech burst

        inst->individual[0] = INDIVIDUAL_10MS_VAG;
        inst->individual[1] = INDIVIDUAL_20MS_VAG;
        inst->individual[2] = INDIVIDUAL_30MS_VAG;

        inst->total[0] = TOTAL_10MS_VAG;
        inst->total[1] = TOTAL_20MS_VAG;
        inst->total[2] = TOTAL_30MS_VAG;
    } else
    {
        return -1;
    }

    return 0;
}

// Calculate VAD decision by first extracting feature values and then calculate
// probability for both speech and background noise.

WebRtc_Word16 WebRtcVad_CalcVad32khz(VadInstT *inst, WebRtc_Word16 *speech_frame,
                                     int frame_length)
{
    WebRtc_Word16 len, vad;
    WebRtc_Word16 speechWB[480]; // Downsampled speech frame: 960 samples (30ms in SWB)
    WebRtc_Word16 speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)


    // Downsample signal 32->16->8 before doing VAD
    WebRtcVad_Downsampling(speech_frame, speechWB, &(inst->downsampling_filter_states[2]),
                           frame_length);
    len = WEBRTC_SPL_RSHIFT_W16(frame_length, 1);

    WebRtcVad_Downsampling(speechWB, speechNB, inst->downsampling_filter_states, len);
    len = WEBRTC_SPL_RSHIFT_W16(len, 1);

    // Do VAD on an 8 kHz signal
    vad = WebRtcVad_CalcVad8khz(inst, speechNB, len);

    return vad;
}

WebRtc_Word16 WebRtcVad_CalcVad16khz(VadInstT *inst, WebRtc_Word16 *speech_frame,
                                     int frame_length)
{
    WebRtc_Word16 len, vad;
    WebRtc_Word16 speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)

    // Wideband: Downsample signal before doing VAD
    WebRtcVad_Downsampling(speech_frame, speechNB, inst->downsampling_filter_states,
                           frame_length);

    len = WEBRTC_SPL_RSHIFT_W16(frame_length, 1);
    vad = WebRtcVad_CalcVad8khz(inst, speechNB, len);

    return vad;
}

WebRtc_Word16 WebRtcVad_CalcVad8khz(VadInstT *inst, WebRtc_Word16 *speech_frame,
                                    int frame_length)
{
    WebRtc_Word16 feature_vector[NUM_CHANNELS], total_power;

    // Get power in the bands
    total_power = WebRtcVad_get_features(inst, speech_frame, frame_length, feature_vector);

    // Make a VAD
    inst->vad = WebRtcVad_GmmProbability(inst, feature_vector, total_power, frame_length);

    return inst->vad;
}

// Calculate probability for both speech and background noise, and perform a
// hypothesis-test.
WebRtc_Word16 WebRtcVad_GmmProbability(VadInstT *inst, WebRtc_Word16 *feature_vector,
                                       WebRtc_Word16 total_power, int frame_length)
{
    int n, k;
    WebRtc_Word16 backval;
    WebRtc_Word16 h0, h1;
    WebRtc_Word16 ratvec, xval;
    WebRtc_Word16 vadflag;
    WebRtc_Word16 shifts0, shifts1;
    WebRtc_Word16 tmp16, tmp16_1, tmp16_2;
    WebRtc_Word16 diff, nr, pos;
    WebRtc_Word16 nmk, nmk2, nmk3, smk, smk2, nsk, ssk;
    WebRtc_Word16 delt, ndelt;
    WebRtc_Word16 maxspe, maxmu;
    WebRtc_Word16 deltaN[NUM_TABLE_VALUES], deltaS[NUM_TABLE_VALUES];
    WebRtc_Word16 ngprvec[NUM_TABLE_VALUES], sgprvec[NUM_TABLE_VALUES];
    WebRtc_Word32 h0test, h1test;
    WebRtc_Word32 tmp32_1, tmp32_2;
    WebRtc_Word32 dotVal;
    WebRtc_Word32 nmid, smid;
    WebRtc_Word32 probn[NUM_MODELS], probs[NUM_MODELS];
    WebRtc_Word16 *nmean1ptr, *nmean2ptr, *smean1ptr, *smean2ptr, *nstd1ptr, *nstd2ptr,
            *sstd1ptr, *sstd2ptr;
    WebRtc_Word16 overhead1, overhead2, individualTest, totalTest;

    // Set the thresholds to different values based on frame length
    if (frame_length == 80)
    {
        // 80 input samples
        overhead1 = inst->over_hang_max_1[0];
        overhead2 = inst->over_hang_max_2[0];
        individualTest = inst->individual[0];
        totalTest = inst->total[0];
    } else if (frame_length == 160)
    {
        // 160 input samples
        overhead1 = inst->over_hang_max_1[1];
        overhead2 = inst->over_hang_max_2[1];
        individualTest = inst->individual[1];
        totalTest = inst->total[1];
    } else
    {
        // 240 input samples
        overhead1 = inst->over_hang_max_1[2];
        overhead2 = inst->over_hang_max_2[2];
        individualTest = inst->individual[2];
        totalTest = inst->total[2];
    }

    if (total_power > MIN_ENERGY)
    { // If signal present at all

        // Set pointers to the gaussian parameters
        nmean1ptr = &inst->noise_means[0];
        nmean2ptr = &inst->noise_means[NUM_CHANNELS];
        smean1ptr = &inst->speech_means[0];
        smean2ptr = &inst->speech_means[NUM_CHANNELS];
        nstd1ptr = &inst->noise_stds[0];
        nstd2ptr = &inst->noise_stds[NUM_CHANNELS];
        sstd1ptr = &inst->speech_stds[0];
        sstd2ptr = &inst->speech_stds[NUM_CHANNELS];

        vadflag = 0;
        dotVal = 0;
        for (n = 0; n < NUM_CHANNELS; n++)
        { // For all channels

            pos = WEBRTC_SPL_LSHIFT_W16(n, 1);
            xval = feature_vector[n];

            // Probability for Noise, Q7 * Q20 = Q27
            tmp32_1 = WebRtcVad_GaussianProbability(xval, *nmean1ptr++, *nstd1ptr++,
                                                    &deltaN[pos]);
            probn[0] = (WebRtc_Word32)(kNoiseDataWeights[n] * tmp32_1);
            tmp32_1 = WebRtcVad_GaussianProbability(xval, *nmean2ptr++, *nstd2ptr++,
                                                    &deltaN[pos + 1]);
            probn[1] = (WebRtc_Word32)(kNoiseDataWeights[n + NUM_CHANNELS] * tmp32_1);
            h0test = probn[0] + probn[1]; // Q27
            h0 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(h0test, 12); // Q15

            // Probability for Speech
            tmp32_1 = WebRtcVad_GaussianProbability(xval, *smean1ptr++, *sstd1ptr++,
                                                    &deltaS[pos]);
            probs[0] = (WebRtc_Word32)(kSpeechDataWeights[n] * tmp32_1);
            tmp32_1 = WebRtcVad_GaussianProbability(xval, *smean2ptr++, *sstd2ptr++,
                                                    &deltaS[pos + 1]);
            probs[1] = (WebRtc_Word32)(kSpeechDataWeights[n + NUM_CHANNELS] * tmp32_1);
            h1test = probs[0] + probs[1]; // Q27
            h1 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(h1test, 12); // Q15

            // Get likelihood ratio. Approximate log2(H1/H0) with shifts0 - shifts1
            shifts0 = WebRtcSpl_NormW32(h0test);
            shifts1 = WebRtcSpl_NormW32(h1test);

            if ((h0test > 0) && (h1test > 0))
            {
                ratvec = shifts0 - shifts1;
            } else if (h1test > 0)
            {
                ratvec = 31 - shifts1;
            } else if (h0test > 0)
            {
                ratvec = shifts0 - 31;
            } else
            {
                ratvec = 0;
            }

            // VAD decision with spectrum weighting
            dotVal += WEBRTC_SPL_MUL_16_16(ratvec, kSpectrumWeight[n]);

            // Individual channel test
            if ((ratvec << 2) > individualTest)
            {
                vadflag = 1;
            }

            // Probabilities used when updating model
            if (h0 > 0)
            {
                tmp32_1 = probn[0] & 0xFFFFF000; // Q27
                tmp32_2 = WEBRTC_SPL_LSHIFT_W32(tmp32_1, 2); // Q29
                ngprvec[pos] = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32_2, h0);
                ngprvec[pos + 1] = 16384 - ngprvec[pos];
            } else
            {
                ngprvec[pos] = 16384;
                ngprvec[pos + 1] = 0;
            }

            // Probabilities used when updating model
            if (h1 > 0)
            {
                tmp32_1 = probs[0] & 0xFFFFF000;
                tmp32_2 = WEBRTC_SPL_LSHIFT_W32(tmp32_1, 2);
                sgprvec[pos] = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32_2, h1);
                sgprvec[pos + 1] = 16384 - sgprvec[pos];
            } else
            {
                sgprvec[pos] = 0;
                sgprvec[pos + 1] = 0;
            }
        }

        // Overall test
        if (dotVal >= totalTest)
        {
            vadflag |= 1;
        }

        // Set pointers to the means and standard deviations.
        nmean1ptr = &inst->noise_means[0];
        smean1ptr = &inst->speech_means[0];
        nstd1ptr = &inst->noise_stds[0];
        sstd1ptr = &inst->speech_stds[0];

        maxspe = 12800;

        // Update the model's parameters
        for (n = 0; n < NUM_CHANNELS; n++)
        {

            pos = WEBRTC_SPL_LSHIFT_W16(n, 1);

            // Get min value in past which is used for long term correction
            backval = WebRtcVad_FindMinimum(inst, feature_vector[n], n); // Q4

            // Compute the "global" mean, that is the sum of the two means weighted
            nmid = WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n], *nmean1ptr); // Q7 * Q7
            nmid += WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n+NUM_CHANNELS],
                    *(nmean1ptr+NUM_CHANNELS));
            tmp16_1 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(nmid, 6); // Q8

            for (k = 0; k < NUM_MODELS; k++)
            {

                nr = pos + k;

                nmean2ptr = nmean1ptr + k * NUM_CHANNELS;
                smean2ptr = smean1ptr + k * NUM_CHANNELS;
                nstd2ptr = nstd1ptr + k * NUM_CHANNELS;
                sstd2ptr = sstd1ptr + k * NUM_CHANNELS;
                nmk = *nmean2ptr;
                smk = *smean2ptr;
                nsk = *nstd2ptr;
                ssk = *sstd2ptr;

                // Update noise mean vector if the frame consists of noise only
                nmk2 = nmk;
                if (!vadflag)
                {
                    // deltaN = (x-mu)/sigma^2
                    // ngprvec[k] = probn[k]/(probn[0] + probn[1])

                    delt = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(ngprvec[nr],
                            deltaN[nr], 11); // Q14*Q11
                    nmk2 = nmk + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(delt,
                            kNoiseUpdateConst,
                            22); // Q7+(Q14*Q15>>22)
                }

                // Long term correction of the noise mean
                ndelt = WEBRTC_SPL_LSHIFT_W16(backval, 4);
                ndelt -= tmp16_1; // Q8 - Q8
                nmk3 = nmk2 + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(ndelt,
                        kBackEta,
                        9); // Q7+(Q8*Q8)>>9

                // Control that the noise mean does not drift to much
                tmp16 = WEBRTC_SPL_LSHIFT_W16(k+5, 7);
                if (nmk3 < tmp16)
                    nmk3 = tmp16;
                tmp16 = WEBRTC_SPL_LSHIFT_W16(72+k-n, 7);
                if (nmk3 > tmp16)
                    nmk3 = tmp16;
                *nmean2ptr = nmk3;

                if (vadflag)
                {
                    // Update speech mean vector:
                    // deltaS = (x-mu)/sigma^2
                    // sgprvec[k] = probn[k]/(probn[0] + probn[1])

                    delt = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(sgprvec[nr],
                            deltaS[nr],
                            11); // (Q14*Q11)>>11=Q14
                    tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(delt,
                            kSpeechUpdateConst,
                            21) + 1;
                    smk2 = smk + (tmp16 >> 1); // Q7 + (Q14 * Q15 >> 22)

                    // Control that the speech mean does not drift to much
                    maxmu = maxspe + 640;
                    if (smk2 < kMinimumMean[k])
                        smk2 = kMinimumMean[k];
                    if (smk2 > maxmu)
                        smk2 = maxmu;

                    *smean2ptr = smk2;

                    // (Q7>>3) = Q4
                    tmp16 = WEBRTC_SPL_RSHIFT_W16((smk + 4), 3);

                    tmp16 = feature_vector[n] - tmp16; // Q4
                    tmp32_1 = WEBRTC_SPL_MUL_16_16_RSFT(deltaS[nr], tmp16, 3);
                    tmp32_2 = tmp32_1 - (WebRtc_Word32)4096; // Q12
                    tmp16 = WEBRTC_SPL_RSHIFT_W16((sgprvec[nr]), 2);
                    tmp32_1 = (WebRtc_Word32)(tmp16 * tmp32_2);// (Q15>>3)*(Q14>>2)=Q12*Q12=Q24

                    tmp32_2 = WEBRTC_SPL_RSHIFT_W32(tmp32_1, 4); // Q20

                    // 0.1 * Q20 / Q7 = Q13
                    if (tmp32_2 > 0)
                        tmp16 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32_2, ssk * 10);
                    else
                    {
                        tmp16 = (WebRtc_Word16)WebRtcSpl_DivW32W16(-tmp32_2, ssk * 10);
                        tmp16 = -tmp16;
                    }
                    // divide by 4 giving an update factor of 0.025
                    tmp16 += 128; // Rounding
                    ssk += WEBRTC_SPL_RSHIFT_W16(tmp16, 8);
                    // Division with 8 plus Q7
                    if (ssk < MIN_STD)
                        ssk = MIN_STD;
                    *sstd2ptr = ssk;
                } else
                {
                    // Update GMM variance vectors
                    // deltaN * (feature_vector[n] - nmk) - 1, Q11 * Q4
                    tmp16 = feature_vector[n] - WEBRTC_SPL_RSHIFT_W16(nmk, 3);

                    // (Q15>>3) * (Q14>>2) = Q12 * Q12 = Q24
                    tmp32_1 = WEBRTC_SPL_MUL_16_16_RSFT(deltaN[nr], tmp16, 3) - 4096;
                    tmp16 = WEBRTC_SPL_RSHIFT_W16((ngprvec[nr]+2), 2);
                    tmp32_2 = (WebRtc_Word32)(tmp16 * tmp32_1);
                    tmp32_1 = WEBRTC_SPL_RSHIFT_W32(tmp32_2, 14);
                    // Q20  * approx 0.001 (2^-10=0.0009766)

                    // Q20 / Q7 = Q13
                    tmp16 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32_1, nsk);
                    if (tmp32_1 > 0)
                        tmp16 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32_1, nsk);
                    else
                    {
                        tmp16 = (WebRtc_Word16)WebRtcSpl_DivW32W16(-tmp32_1, nsk);
                        tmp16 = -tmp16;
                    }
                    tmp16 += 32; // Rounding
                    nsk += WEBRTC_SPL_RSHIFT_W16(tmp16, 6);

                    if (nsk < MIN_STD)
                        nsk = MIN_STD;

                    *nstd2ptr = nsk;
                }
            }

            // Separate models if they are too close - nmid in Q14
            nmid = WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n], *nmean1ptr);
            nmid += WEBRTC_SPL_MUL_16_16(kNoiseDataWeights[n+NUM_CHANNELS], *nmean2ptr);

            // smid in Q14
            smid = WEBRTC_SPL_MUL_16_16(kSpeechDataWeights[n], *smean1ptr);
            smid += WEBRTC_SPL_MUL_16_16(kSpeechDataWeights[n+NUM_CHANNELS], *smean2ptr);

            // diff = "global" speech mean - "global" noise mean
            diff = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(smid, 9);
            tmp16 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(nmid, 9);
            diff -= tmp16;

            if (diff < kMinimumDifference[n])
            {

                tmp16 = kMinimumDifference[n] - diff; // Q5

                // tmp16_1 = ~0.8 * (kMinimumDifference - diff) in Q7
                // tmp16_2 = ~0.2 * (kMinimumDifference - diff) in Q7
                tmp16_1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(13, tmp16, 2);
                tmp16_2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(3, tmp16, 2);

                // First Gauss, speech model
                tmp16 = tmp16_1 + *smean1ptr;
                *smean1ptr = tmp16;
                smid = WEBRTC_SPL_MUL_16_16(tmp16, kSpeechDataWeights[n]);

                // Second Gauss, speech model
                tmp16 = tmp16_1 + *smean2ptr;
                *smean2ptr = tmp16;
                smid += WEBRTC_SPL_MUL_16_16(tmp16, kSpeechDataWeights[n+NUM_CHANNELS]);

                // First Gauss, noise model
                tmp16 = *nmean1ptr - tmp16_2;
                *nmean1ptr = tmp16;

                nmid = WEBRTC_SPL_MUL_16_16(tmp16, kNoiseDataWeights[n]);

                // Second Gauss, noise model
                tmp16 = *nmean2ptr - tmp16_2;
                *nmean2ptr = tmp16;
                nmid += WEBRTC_SPL_MUL_16_16(tmp16, kNoiseDataWeights[n+NUM_CHANNELS]);
            }

            // Control that the speech & noise means do not drift to much
            maxspe = kMaximumSpeech[n];
            tmp16_2 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(smid, 7);
            if (tmp16_2 > maxspe)
            { // Upper limit of speech model
                tmp16_2 -= maxspe;

                *smean1ptr -= tmp16_2;
                *smean2ptr -= tmp16_2;
            }

            tmp16_2 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(nmid, 7);
            if (tmp16_2 > kMaximumNoise[n])
            {
                tmp16_2 -= kMaximumNoise[n];

                *nmean1ptr -= tmp16_2;
                *nmean2ptr -= tmp16_2;
            }

            *nmean1ptr++;
            *smean1ptr++;
            *nstd1ptr++;
            *sstd1ptr++;
        }
        inst->frame_counter++;
    } else
    {
        vadflag = 0;
    }

    // Hangover smoothing
    if (!vadflag)
    {
        if (inst->over_hang > 0)
        {
            vadflag = 2 + inst->over_hang;
            inst->over_hang = inst->over_hang - 1;
        }
        inst->num_of_speech = 0;
    } else
    {
        inst->num_of_speech = inst->num_of_speech + 1;
        if (inst->num_of_speech > NSP_MAX)
        {
            inst->num_of_speech = NSP_MAX;
            inst->over_hang = overhead2;
        } else
            inst->over_hang = overhead1;
    }
    return vadflag;
}
