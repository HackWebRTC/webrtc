#include "modules/transit_media/file_capturer.h"

#if defined(WEBRTC_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
}

#include "common_video/h264/h264_common.h"
#include "modules/transit_media/transit_video_frame_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/time_utils.h"

#define DEBUG_LOG 0

namespace webrtc {

FileCapturer::FileCapturer(const std::string& path,
                           const std::string& dump_path,
                           TaskQueueFactory* task_queue_factory)
    : queue_(task_queue_factory->CreateTaskQueue(
          "FileCapturer",
          TaskQueueFactory::Priority::NORMAL)),
      path_(path),
      dump_path_(dump_path),
      running_(false),
      video_callback_(nullptr) {}

FileCapturer::~FileCapturer() {
  // Delete and and thus stop task queue before deleting other members to avoid
  // race with running tasks.
  queue_.get_deleter()(queue_.get());
  queue_.release();
  Stop();
}

int FileCapturer::Start(bool loop) {
  RTC_LOG(LS_INFO) << "FileCapturer::Start *" << path_.c_str() << "*, *" << dump_path_.c_str() << "*, loop " << loop;
  if (running_) {
    RTC_LOG(LS_INFO) << "FileCapturer::Start, already started";
    return 0;
  }

  AVFormatContext* fmt_ctx = nullptr;
  int error = avformat_open_input(&fmt_ctx, path_.c_str(), nullptr, nullptr);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start avformat_open_input fail "
                      << path_.c_str() << " " << av_err2str(error);
    return -1;
  }

  error = avformat_find_stream_info(fmt_ctx, nullptr);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start avformat_find_stream_info fail "
                      << av_err2str(error);
    avformat_close_input(&fmt_ctx);
    return -2;
  }
  int v_stream_no = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1,
                                        -1, nullptr, 0);
  if (v_stream_no < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_find_best_stream fail "
                      << av_err2str(v_stream_no);
    avformat_close_input(&fmt_ctx);
    return -3;
  }
  AVStream* v_stream = fmt_ctx->streams[v_stream_no];
  if (v_stream->codecpar->codec_id != AV_CODEC_ID_H264 && v_stream->codecpar->codec_id != AV_CODEC_ID_HEVC) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start video codec unsupported "
                      << v_stream->codecpar->codec_id;
    avformat_close_input(&fmt_ctx);
    return -4;
  }

  bool h264 = v_stream->codecpar->codec_id == AV_CODEC_ID_H264;
  const AVBitStreamFilter *bsf = av_bsf_get_by_name(h264 ? "h264_mp4toannexb" : "hevc_mp4toannexb");
  if (!bsf) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start fail to get bsf "
                      << h264 ? "h264_mp4toannexb" : "hevc_mp4toannexb";
    avformat_close_input(&fmt_ctx);
    return -5;
  }
  AVBSFContext *bsf_ctx = nullptr;
  error = av_bsf_alloc(bsf, &bsf_ctx);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_bsf_alloc fail "
                      << av_err2str(error);
    avformat_close_input(&fmt_ctx);
    return -6;
  }
  error = avcodec_parameters_copy(bsf_ctx->par_in, v_stream->codecpar);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start avcodec_parameters_copy fail "
                      << av_err2str(error);
    av_bsf_free(&bsf_ctx);
    avformat_close_input(&fmt_ctx);
    return -7;
  }
  error = av_bsf_init(bsf_ctx);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_bsf_init fail "
                      << av_err2str(error);
    av_bsf_free(&bsf_ctx);
    avformat_close_input(&fmt_ctx);
    return -8;
  }

  AVPacket* pkt_in = av_packet_alloc();
  AVPacket* pkt_out = av_packet_alloc();
  if (!pkt_in || !pkt_out) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_packet_alloc fail";
    av_bsf_free(&bsf_ctx);
    avformat_close_input(&fmt_ctx);
    return -9;
  }

#if DEBUG_LOG
  av_dump_format(fmt_ctx, 0, path_.c_str(), 0);
  RTC_LOG(LS_INFO) << "fmt_ctx->nb_streams " << fmt_ctx->nb_streams
      << ", index " << v_stream->index
      << ", id " << v_stream->id
      << ", start_time " << v_stream->start_time
      << ", duration " << v_stream->duration
      << ", nb_frames " << v_stream->nb_frames
      << ", time_base.num " << v_stream->time_base.num
      << ", time_base.den " << v_stream->time_base.den;
#endif

  running_ = true;
  queue_->PostTask([this, fmt_ctx, bsf_ctx, v_stream, pkt_in, pkt_out, loop]() mutable {
    bool first_packet = true;
    int64_t last_pts_ms = 0;
    int64_t emit_next_packet_ms = 0;
    int width = v_stream->codecpar->width;
    int height = v_stream->codecpar->height;
    bool need_loop_again = false;
    RTC_LOG(LS_INFO) << "FileCapturer::Start " << path_.c_str() << " success, " << width << "x" << height;

    FILE* dump = nullptr;
    if (!dump_path_.empty()) {
      dump = fopen(dump_path_.c_str(), "wb");
    }

    while (running_) {
      int ret = av_read_frame(fmt_ctx, pkt_in);
      if (ret < 0 && ret != AVERROR_EOF) {
        RTC_LOG(LS_ERROR) << "FileCapturer::Start av_read_frame fail "
                          << av_err2str(ret);
        running_ = false;
        break;
      }
      if (ret == AVERROR_EOF) {
        if (loop) {
          RTC_LOG(LS_INFO) << "FileCapturer::Start got EOF, loop again";
          running_ = false;
          need_loop_again = true;
          break;
        } else {
          RTC_LOG(LS_INFO) << "FileCapturer::Start got EOF, quit";
          running_ = false;
          break;
        }
      }
      if (pkt_in->stream_index != v_stream->index) {
        av_packet_unref(pkt_in);
        continue;
      }

      int64_t now_ms = rtc::TimeMicros() / 1000;
      int64_t this_pts_ms = 1000 * pkt_in->pts * v_stream->time_base.num /
                            v_stream->time_base.den;
      if (first_packet) {
        emit_next_packet_ms = now_ms;
        last_pts_ms = this_pts_ms;
      }
      emit_next_packet_ms += this_pts_ms - last_pts_ms;
      last_pts_ms = this_pts_ms;
      int sleep_ms = std::min(60, (int)(emit_next_packet_ms - now_ms));

#if DEBUG_LOG
      RTC_LOG(LS_INFO) << "FileCapturer::Start av_read_frame ret " << ret << " " << pkt_in->size
                      << " " << pkt_in->stream_index << " " << v_stream->index
                      << ", pkt_in->pts " << pkt_in->pts
                      << ", v_stream->time_base.num " << v_stream->time_base.num
                      << ", v_stream->time_base.den " << v_stream->time_base.den
                      << ", this_pts_ms " << this_pts_ms
                      << ", last_pts_ms " << last_pts_ms
                      << ", emit_next_packet_ms " << emit_next_packet_ms
                      << ", now_ms " << now_ms
                      << ", sleep_ms " << sleep_ms;

      rtc::ArrayView<const uint8_t> array_view(pkt_in->data, pkt_in->size);
      std::vector<webrtc::H264::NaluIndex> indices = webrtc::H264::FindNaluIndices(array_view);

      for (size_t i = 0; i < indices.size(); i++) {
          H264::NaluType type = H264::ParseNaluType(
              pkt_in->data[indices[i].payload_start_offset]);
          RTC_LOG(LS_INFO) << "FileCapturer::Start nalu[" << i
                            << "] type " << type;
      }
#endif

      if (!first_packet && sleep_ms > 10) {
#if defined(WEBRTC_WIN)
        Sleep(sleep_ms);
#else
        usleep(sleep_ms * 1000);
#endif
      }
      first_packet = false;

      ret = av_bsf_send_packet(bsf_ctx, pkt_in);
      if (ret < 0 && ret != AVERROR(EAGAIN)) {
        RTC_LOG(LS_ERROR) << "FileCapturer::Start av_bsf_send_packet fail "
                          << av_err2str(ret);
        av_packet_unref(pkt_in);
        running_ = false;
        break;
      }
      bool retry_bsf = ret == AVERROR(EAGAIN);
      if (!retry_bsf) {
        av_packet_unref(pkt_in);
      }

      while (true) {
        ret = av_bsf_receive_packet(bsf_ctx, pkt_out);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
          RTC_LOG(LS_ERROR) << "FileCapturer::Start av_bsf_receive_packet fail "
                            << av_err2str(ret);
          av_packet_unref(pkt_out);
          running_ = false;
          break;
        }
        if (ret == AVERROR(EAGAIN)) {
          break;
        }

        if (dump) {
          fwrite(pkt_out->data, 1, pkt_out->size, dump);
        }
  
        if (video_callback_ && running_) {
          rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer> frame_buffer =
              rtc::make_ref_counted<webrtc::TransitVideoFrameBuffer>(
                  width, height, pkt_out->size);
          memcpy(frame_buffer->mutable_data(), pkt_out->data, pkt_out->size);
  
          video_callback_->OnFrame(
              VideoFrame::Builder()
                  .set_video_frame_buffer(frame_buffer)
                  .set_rotation(VideoRotation::kVideoRotation_0)
                  .set_dummy(false)
                  .set_transit(true)
                  .set_rtp_timestamp(0)
                  .set_timestamp_ms(rtc::TimeMillis())
                  .build());
        }

        av_packet_unref(pkt_out);
      }

      if (!running_) {
        av_packet_unref(pkt_in);
        break;
      }
      if (retry_bsf) {
        ret = av_bsf_send_packet(bsf_ctx, pkt_in);
        av_packet_unref(pkt_in);
        if (ret < 0) {
          RTC_LOG(LS_ERROR) << "FileCapturer::Start retry av_bsf_send_packet fail "
                            << av_err2str(ret);
          running_ = false;
          break;
        }
      }
    }

    if (dump) {
      fclose(dump);
      dump = nullptr;
    }
    av_packet_free(&pkt_in);
    av_packet_free(&pkt_out);
    av_bsf_free(&bsf_ctx);
    avformat_close_input(&fmt_ctx);

    if (need_loop_again) {
      Start(true);
    }
  });

  RTC_LOG(LS_INFO) << "FileCapturer::Start success";
  return 0;
}

void FileCapturer::Stop() {
  RTC_LOG(LS_INFO) << "FileCapturer::Stop";
  running_ = false;
}

}  // namespace webrtc
