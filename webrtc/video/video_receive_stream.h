/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include <memory>
#include <vector>

#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/call/syncable.h"
#include "webrtc/call/video_receive_stream.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"
#include "webrtc/modules/video_coding/frame_buffer2.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/rtc_base/sequenced_task_checker.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video/rtp_streams_synchronizer.h"
#include "webrtc/video/rtp_video_stream_receiver.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video/video_stream_decoder.h"

namespace webrtc {

class CallStats;
class IvfFileWriter;
class ProcessThread;
class RTPFragmentationHeader;
class RtpStreamReceiverInterface;
class RtpStreamReceiverControllerInterface;
class RtxReceiveStream;
class VCMTiming;
class VCMJitterEstimator;

namespace internal {

class VideoReceiveStream : public webrtc::VideoReceiveStream,
                           public rtc::VideoSinkInterface<VideoFrame>,
                           public EncodedImageCallback,
                           public NackSender,
                           public KeyFrameRequestSender,
                           public video_coding::OnCompleteFrameCallback,
                           public Syncable {
 public:
  VideoReceiveStream(RtpStreamReceiverControllerInterface* receiver_controller,
                     int num_cpu_cores,
                     PacketRouter* packet_router,
                     VideoReceiveStream::Config config,
                     ProcessThread* process_thread,
                     CallStats* call_stats);
  ~VideoReceiveStream() override;

  const Config& config() const { return config_; }

  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);

  void SetSync(Syncable* audio_syncable);

  // Implements webrtc::VideoReceiveStream.
  void Start() override;
  void Stop() override;

  webrtc::VideoReceiveStream::Stats GetStats() const override;

  // Takes ownership of the file, is responsible for closing it later.
  // Calling this method will close and finalize any current log.
  // Giving rtc::kInvalidPlatformFileValue disables logging.
  // If a frame to be written would make the log too large the write fails and
  // the log is closed and finalized. A |byte_limit| of 0 means no limit.
  void EnableEncodedFrameRecording(rtc::PlatformFile file,
                                   size_t byte_limit) override;

  void AddSecondarySink(RtpPacketSinkInterface* sink) override;
  void RemoveSecondarySink(const RtpPacketSinkInterface* sink) override;

  // Implements rtc::VideoSinkInterface<VideoFrame>.
  void OnFrame(const VideoFrame& video_frame) override;

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  // Implements NackSender.
  void SendNack(const std::vector<uint16_t>& sequence_numbers) override;

  // Implements KeyFrameRequestSender.
  void RequestKeyFrame() override;

  // Implements video_coding::OnCompleteFrameCallback.
  void OnCompleteFrame(
      std::unique_ptr<video_coding::FrameObject> frame) override;

  // Implements Syncable.
  int id() const override;
  rtc::Optional<Syncable::Info> GetInfo() const override;
  uint32_t GetPlayoutTimestamp() const override;
  void SetMinimumPlayoutDelay(int delay_ms) override;

 private:
  static void DecodeThreadFunction(void* ptr);
  bool Decode();

  rtc::SequencedTaskChecker worker_sequence_checker_;
  rtc::SequencedTaskChecker module_process_sequence_checker_;

  TransportAdapter transport_adapter_;
  const VideoReceiveStream::Config config_;
  const int num_cpu_cores_;
  ProcessThread* const process_thread_;
  Clock* const clock_;

  rtc::PlatformThread decode_thread_;

  CallStats* const call_stats_;

  // Shared by media and rtx stream receivers, since the latter has no RtpRtcp
  // module of its own.
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;

  std::unique_ptr<VCMTiming> timing_;  // Jitter buffer experiment.
  vcm::VideoReceiver video_receiver_;
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> incoming_video_stream_;
  ReceiveStatisticsProxy stats_proxy_;
  RtpVideoStreamReceiver rtp_video_stream_receiver_;
  std::unique_ptr<VideoStreamDecoder> video_stream_decoder_;
  RtpStreamsSynchronizer rtp_stream_sync_;

  rtc::CriticalSection ivf_writer_lock_;
  std::unique_ptr<IvfFileWriter> ivf_writer_ RTC_GUARDED_BY(ivf_writer_lock_);

  // Members for the new jitter buffer experiment.
  std::unique_ptr<VCMJitterEstimator> jitter_estimator_;
  std::unique_ptr<video_coding::FrameBuffer> frame_buffer_;

  std::unique_ptr<RtpStreamReceiverInterface> media_receiver_;
  std::unique_ptr<RtxReceiveStream> rtx_receive_stream_;
  std::unique_ptr<RtpStreamReceiverInterface> rtx_receiver_;

  // Whenever we are in an undecodable state (stream has just started or due to
  // a decoding error) we require a keyframe to restart the stream.
  bool keyframe_required_ = true;

  // If we have successfully decoded any frame.
  bool frame_decoded_ = false;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
