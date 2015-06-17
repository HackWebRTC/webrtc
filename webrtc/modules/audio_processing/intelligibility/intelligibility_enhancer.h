/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_

#include <complex>

#include "webrtc/common_audio/lapped_transform.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

struct WebRtcVadInst;
typedef struct WebRtcVadInst VadInst;

namespace webrtc {

// Speech intelligibility enhancement module. Reads render and capture
// audio streams and modifies the render stream with a set of gains per
// frequency bin to enhance speech against the noise background.
class IntelligibilityEnhancer {
 public:
  // Construct a new instance with the given filter bank resolution,
  // sampling rate, number of channels and analysis rates.
  // |analysis_rate| sets the number of input blocks (containing speech!)
  // to elapse before a new gain computation is made. |variance_rate| specifies
  // the number of gain recomputations after which the variances are reset.
  // |cv_*| are parameters for the VarianceArray constructor for the
  // lear speech stream.
  // TODO(bercic): the |cv_*|, |*_rate| and |gain_limit| parameters should
  // probably go away once fine tuning is done. They override the internal
  // constants in the class (kGainChangeLimit, kAnalyzeRate, kVarianceRate).
  IntelligibilityEnhancer(int erb_resolution, int sample_rate_hz, int channels,
                          int cv_type, float cv_alpha, int cv_win,
                          int analysis_rate, int variance_rate,
                          float gain_limit);
  ~IntelligibilityEnhancer();

  void ProcessRenderAudio(float* const* audio);
  void ProcessCaptureAudio(float* const* audio);

 private:
  enum AudioSource {
    kRenderStream = 0,
    kCaptureStream,
  };

  class TransformCallback : public LappedTransform::Callback {
   public:
    TransformCallback(IntelligibilityEnhancer* parent, AudioSource source);
    virtual void ProcessAudioBlock(const std::complex<float>* const* in_block,
                                   int in_channels, int frames,
                                   int out_channels,
                                   std::complex<float>* const* out_block);

   private:
    IntelligibilityEnhancer* parent_;
    AudioSource source_;
  };
  friend class TransformCallback;

  void DispatchAudio(AudioSource source, const std::complex<float>* in_block,
                     std::complex<float>* out_block);
  void ProcessClearBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);
  void AnalyzeClearBlock(float power_target);
  void ProcessNoiseBlock(const std::complex<float>* in_block,
                         std::complex<float>* out_block);

  static int GetBankSize(int sample_rate, int erb_resolution);
  void CreateErbBank();
  void SolveEquation14(float lambda, int start_freq, float* sols);
  void FilterVariance(const float* var, float* result);
  static float DotProduct(const float* a, const float* b, int length);

  static const int kErbResolution;
  static const int kWindowSizeMs;
  static const int kChunkSizeMs;
  static const int kAnalyzeRate;
  static const int kVarianceRate;
  static const float kClipFreq;
  static const float kConfigRho;
  static const float kKbdAlpha;
  static const float kGainChangeLimit;

  const int freqs_;
  const int window_size_;  // window size in samples; also the block size
  const int chunk_length_;  // chunk size in samples
  const int bank_size_;
  const int sample_rate_hz_;
  const int erb_resolution_;
  const int channels_;
  const int analysis_rate_;
  const int variance_rate_;

  intelligibility::VarianceArray clear_variance_;
  intelligibility::VarianceArray noise_variance_;
  scoped_ptr<float[]> filtered_clear_var_;
  scoped_ptr<float[]> filtered_noise_var_;
  float** filter_bank_;
  scoped_ptr<float[]> center_freqs_;
  int start_freq_;
  scoped_ptr<float[]> rho_;
  scoped_ptr<float[]> gains_eq_;
  intelligibility::GainApplier gain_applier_;

  // Destination buffer used to reassemble blocked chunks before overwriting
  // the original input array with modifications.
  float** temp_out_buffer_;
  scoped_ptr<float*[]> input_audio_;
  scoped_ptr<float[]> kbd_window_;
  TransformCallback render_callback_;
  TransformCallback capture_callback_;
  scoped_ptr<LappedTransform> render_mangler_;
  scoped_ptr<LappedTransform> capture_mangler_;
  int block_count_;
  int analysis_step_;

  // TODO(bercic): Quick stopgap measure for voice detection in the clear
  // and noise streams.
  VadInst* vad_high_;
  VadInst* vad_low_;
  scoped_ptr<int16_t[]> vad_tmp_buffer_;
  bool has_voice_low_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_ENHANCER_H_

