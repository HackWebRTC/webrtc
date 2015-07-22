/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Specifies core class for intelligbility enhancement.
//

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_

#include <complex>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/lapped_transform.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

struct WebRtcVadInst;
typedef struct WebRtcVadInst VadInst;

namespace webrtc {

// Speech intelligibility enhancement module. Reads render and capture
// audio streams and modifies the render stream with a set of gains per
// frequency bin to enhance speech against the noise background.
// Note: assumes speech and noise streams are already separated.
class IntelligibilityEnhancer {
 public:
  // Construct a new instance with the given filter bank resolution,
  // sampling rate, number of channels and analysis rates.
  // |analysis_rate| sets the number of input blocks (containing speech!)
  // to elapse before a new gain computation is made. |variance_rate| specifies
  // the number of gain recomputations after which the variances are reset.
  // |cv_*| are parameters for the VarianceArray constructor for the
  // clear speech stream.
  // TODO(bercic): the |cv_*|, |*_rate| and |gain_limit| parameters should
  // probably go away once fine tuning is done. They override the internal
  // constants in the class (kGainChangeLimit, kAnalyzeRate, kVarianceRate).
  IntelligibilityEnhancer(int erb_resolution,
                          int sample_rate_hz,
                          int channels,
                          int cv_type,
                          float cv_alpha,
                          int cv_win,
                          int analysis_rate,
                          int variance_rate,
                          float gain_limit);
  ~IntelligibilityEnhancer();

  // Reads and processes chunk of noise stream in time domain.
  void ProcessCaptureAudio(float* const* audio);

  // Reads chunk of speech in time domain and updates with modified signal.
  void ProcessRenderAudio(float* const* audio);

 private:
  enum AudioSource {
    kRenderStream = 0,  // Clear speech stream.
    kCaptureStream,  // Noise stream.
  };

  // Provides access point to the frequency domain.
  class TransformCallback : public LappedTransform::Callback {
   public:
    TransformCallback(IntelligibilityEnhancer* parent, AudioSource source);

    // All in frequency domain, receives input |in_block|, applies
    // intelligibility enhancement, and writes result to |out_block|.
    void ProcessAudioBlock(const std::complex<float>* const* in_block,
                           int in_channels,
                           int frames,
                           int out_channels,
                           std::complex<float>* const* out_block) override;

   private:
    IntelligibilityEnhancer* parent_;
    AudioSource source_;
  };
  friend class TransformCallback;
  FRIEND_TEST_ALL_PREFIXES(IntelligibilityEnhancerTest, TestErbCreation);
  FRIEND_TEST_ALL_PREFIXES(IntelligibilityEnhancerTest, TestSolveForGains);

  // Sends streams to ProcessClearBlock or ProcessNoiseBlock based on source.
  void DispatchAudio(AudioSource source,
                     const std::complex<float>* in_block,
                     std::complex<float>* out_block);

  // Updates variance computation and analysis with |in_block_|,
  // and writes modified speech to |out_block|.
  void ProcessClearBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);

  // Computes and sets modified gains.
  void AnalyzeClearBlock(float power_target);

  // Bisection search for optimal |lambda|.
  void SolveForLambda(float power_target, float power_bot, float power_top);

  // Transforms freq gains to ERB gains.
  void UpdateErbGains();

  // Updates variance calculation for noise input with |in_block|.
  void ProcessNoiseBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);

  // Returns number of ERB filters.
  static int GetBankSize(int sample_rate, int erb_resolution);

  // Initializes ERB filterbank.
  void CreateErbBank();

  // Analytically solves quadratic for optimal gains given |lambda|.
  // Negative gains are set to 0. Stores the results in |sols|.
  void SolveForGainsGivenLambda(float lambda, int start_freq, float* sols);

  // Computes variance across ERB filters from freq variance |var|.
  // Stores in |result|.
  void FilterVariance(const float* var, float* result);

  // Returns dot product of vectors specified by size |length| arrays |a|,|b|.
  static float DotProduct(const float* a, const float* b, int length);

  const int freqs_;         // Num frequencies in frequency domain.
  const int window_size_;   // Window size in samples; also the block size.
  const int chunk_length_;  // Chunk size in samples.
  const int bank_size_;     // Num ERB filters.
  const int sample_rate_hz_;
  const int erb_resolution_;
  const int channels_;       // Num channels.
  const int analysis_rate_;  // Num blocks before gains recalculated.
  const int variance_rate_;  // Num recalculations before history is cleared.

  intelligibility::VarianceArray clear_variance_;
  intelligibility::VarianceArray noise_variance_;
  rtc::scoped_ptr<float[]> filtered_clear_var_;
  rtc::scoped_ptr<float[]> filtered_noise_var_;
  std::vector<std::vector<float>> filter_bank_;
  rtc::scoped_ptr<float[]> center_freqs_;
  int start_freq_;
  rtc::scoped_ptr<float[]> rho_;  // Production and interpretation SNR.
                                  // for each ERB band.
  rtc::scoped_ptr<float[]> gains_eq_;  // Pre-filter modified gains.
  intelligibility::GainApplier gain_applier_;

  // Destination buffer used to reassemble blocked chunks before overwriting
  // the original input array with modifications.
  // TODO(ekmeyerson): Switch to using ChannelBuffer.
  float** temp_out_buffer_;

  rtc::scoped_ptr<float* []> input_audio_;
  rtc::scoped_ptr<float[]> kbd_window_;
  TransformCallback render_callback_;
  TransformCallback capture_callback_;
  rtc::scoped_ptr<LappedTransform> render_mangler_;
  rtc::scoped_ptr<LappedTransform> capture_mangler_;
  int block_count_;
  int analysis_step_;

  // TODO(bercic): Quick stopgap measure for voice detection in the clear
  // and noise streams.
  // Note: VAD currently does not affect anything in IntelligibilityEnhancer.
  VadInst* vad_high_;
  VadInst* vad_low_;
  rtc::scoped_ptr<int16_t[]> vad_tmp_buffer_;
  bool has_voice_low_;  // Whether voice detected in speech stream.
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_
