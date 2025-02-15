#include "modules/transit_media/file_capturer.h"

#if defined(WEBRTC_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "common_video/h264/h264_common.h"
#include "modules/transit_media/transit_video_frame_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

FileCapturer::FileCapturer(const std::string& path,
                           const std::string& dump_path,
                           int width,
                           int height,
                           TaskQueueFactory* task_queue_factory)
    : queue_(task_queue_factory->CreateTaskQueue(
          "FileCapturer",
          TaskQueueFactory::Priority::NORMAL)),
      path_(path),
      dump_path_(dump_path),
      width_(width),
      height_(height),
      running_(false),
      video_callback_(nullptr) {}

FileCapturer::~FileCapturer() {
  Stop();
}

int FileCapturer::Start(bool loop) {
  RTC_LOG(LS_INFO) << "FileCapturer::Start loop " << loop;
  if (running_) {
    RTC_LOG(LS_INFO) << "FileCapturer::Start, already started";
    return 0;
  }
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_packet_alloc fail";
    return -1;
  }
  av_init_packet(packet);

  AVFormatContext* format_context = nullptr;
  int error =
      avformat_open_input(&format_context, path_.c_str(), nullptr, nullptr);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start avformat_open_input fail "
                      << path_.c_str() << " " << av_err2str(error);
    av_packet_free(&packet);
    return -2;
  }

  error = avformat_find_stream_info(format_context, nullptr);
  if (error < 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start avformat_find_stream_info fail "
                      << av_err2str(error);
    av_packet_free(&packet);
    avformat_close_input(&format_context);
    return -3;
  }

  int v_stream_no = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1,
                                        -1, nullptr, 0);
  if (v_stream_no < 0 ||
      format_context->streams[v_stream_no]->time_base.den <= 0) {
    RTC_LOG(LS_ERROR) << "FileCapturer::Start av_find_best_stream fail "
                      << av_err2str(v_stream_no);
    av_packet_free(&packet);
    avformat_close_input(&format_context);
    return -4;
  }
  AVStream* v_stream = format_context->streams[v_stream_no];

  running_ = true;
  queue_.PostTask([this, format_context, v_stream, packet, loop]() mutable {
    bool first_packet = true;
    int64_t last_pts_ms = 0;
    int64_t emit_next_packet_ms = 0;
    FILE* dump = nullptr;
    if (!dump_path_.empty()) {
      fopen(dump_path_.c_str(), "wb");
    }
    while (running_) {
      int ret = av_read_frame(format_context, packet);
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
          Start(true);
          break;
        } else {
          RTC_LOG(LS_INFO) << "FileCapturer::Start got EOF, quit";
          running_ = false;
          break;
        }
      }
      if (packet->stream_index != v_stream->index) {
        continue;
      }
      int64_t now_ms = rtc::TimeMicros() / 1000;
      int64_t this_pts_ms = 1000 * packet->pts * v_stream->time_base.num /
                            v_stream->time_base.den;
      if (first_packet) {
        emit_next_packet_ms = now_ms;
        last_pts_ms = this_pts_ms;
      }
      emit_next_packet_ms += this_pts_ms - last_pts_ms;
      last_pts_ms = this_pts_ms;
      int sleep_ms = (int)(emit_next_packet_ms - now_ms);
      if (!first_packet && sleep_ms > 5) {
#if defined(WEBRTC_WIN)
        Sleep(sleep_ms);
#else
        usleep(sleep_ms * 1000);
#endif
      }

      if (dump) {
        fwrite(packet->data, 1, packet->size, dump);
      }

#if 0
      RTC_LOG(LS_INFO) << "FileCapturer::Start got packet, pts_ms "
                        << this_pts_ms << ", size " << packet->size;

      std::vector<webrtc::H264::NaluIndex> indices =
          webrtc::H264::FindNaluIndices(packet->data, packet->size);

      for (size_t i = 0; i < indices.size(); i++) {
          H264::NaluType type = H264::ParseNaluType(
              packet->data[indices[i].payload_start_offset]);
          RTC_LOG(LS_INFO) << "FileCapturer::Start nalu[" << i
                            << "] type " << type;
      }
#endif

      first_packet = false;
      if (video_callback_ && running_) {
        rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer> frame_buffer =
            new rtc::RefCountedObject<webrtc::TransitVideoFrameBuffer>(
                width_, height_, packet->size);
        memcpy(frame_buffer->mutable_data(), packet->data, packet->size);

        video_callback_->OnFrame(
            VideoFrame::Builder()
                .set_video_frame_buffer(frame_buffer)
                .set_rotation(VideoRotation::kVideoRotation_0)
                .set_dummy(false)
                .set_transit(true)
                .set_timestamp_us(rtc::TimeMicros())
                .build());
      }
    }
    if (dump) {
      fclose(dump);
    }
    av_packet_free(&packet);
    avformat_close_input(&format_context);
  });

  RTC_LOG(LS_INFO) << "FileCapturer::Start success";
  return 0;
}

void FileCapturer::Stop() {
  RTC_LOG(LS_INFO) << "FileCapturer::Stop";
  running_ = false;
}

}  // namespace webrtc
