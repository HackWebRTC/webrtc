//
// Created by Piasy on 04/11/2017.
//

#include "modules/backing_track/audio_resampler.h"
#include "modules/backing_track/audio_mixer_global.h"
#include "rtc_base/logging.h"

namespace webrtc {

AudioResampler::AudioResampler(AVSampleFormat input_format,
                               int32_t input_sample_rate,
                               int32_t input_channel_num,
                               AVSampleFormat output_format,
                               int32_t output_sample_rate,
                               int32_t output_channel_num)
    : context_(swr_alloc()),
      input_format_(input_format),
      input_sample_rate_(input_sample_rate),
      input_channel_num_(input_channel_num),
      output_format_(output_format),
      output_sample_rate_(output_sample_rate),
      output_channel_num_(output_channel_num) {
    if (!context_) {
        RTC_LOG(LS_ERROR) << "AudioResampler:: swr_alloc fail";
        return;
    }
    int64_t input_channel_layout =
        (input_channel_num_ == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    int64_t output_channel_layout =
        (output_channel_num_ == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

    av_opt_set_int(context_.get(), "in_channel_layout", input_channel_layout,
                   0);
    av_opt_set_int(context_.get(), "in_sample_rate", input_sample_rate_, 0);
    av_opt_set_sample_fmt(context_.get(), "in_sample_fmt", input_format_, 0);

    av_opt_set_int(context_.get(), "out_channel_layout", output_channel_layout,
                   0);
    av_opt_set_int(context_.get(), "out_sample_rate", output_sample_rate_, 0);
    av_opt_set_sample_fmt(context_.get(), "out_sample_fmt", output_format_, 0);

    int32_t error = swr_init(context_.get());
    if (error < 0) {
        context_.reset();
        RTC_LOG(LS_ERROR) << "AudioResampler swr_init fail: "
                          << av_err2str(error);
    }
}

int32_t AudioResampler::Resample(void** input_buffer, int32_t input_size,
                                 void** output_buffer) {
    if (!context_ || input_channel_num_ <= 0
        || av_get_bytes_per_sample(input_format_) <= 0) {
        return kMixerErrInit;
    }

    int32_t input_samples = input_size / input_channel_num_ /
                            av_get_bytes_per_sample(input_format_);
    int32_t output_samples = static_cast<int>(av_rescale_rnd(
        input_samples, output_sample_rate_, input_sample_rate_, AV_ROUND_UP));

    int32_t real_output_samples = swr_convert(
        context_.get(), reinterpret_cast<uint8_t**>(output_buffer),
        output_samples, (const uint8_t**)input_buffer, input_samples);

    if (real_output_samples < 0) {
        return kMixerErrResample;
    }

    return real_output_samples * av_get_bytes_per_sample(output_format_) *
           output_channel_num_;
}

int32_t AudioResampler::CalcOutputSize(int32_t input_size) {
    if (input_channel_num_ <= 0
        || av_get_bytes_per_sample(input_format_) <= 0) {
        return kMixerErrInit;
    }

    int32_t input_samples = input_size / input_channel_num_ /
                            av_get_bytes_per_sample(input_format_);
    int32_t output_samples = static_cast<int>(av_rescale_rnd(
        input_samples, output_sample_rate_, input_sample_rate_, AV_ROUND_UP));
    return output_samples * input_channel_num_ *
           av_get_bytes_per_sample(input_format_);
}
}
