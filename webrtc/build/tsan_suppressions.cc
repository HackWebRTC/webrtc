/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the WebRTC suppressions for ThreadSanitizer.
// Please refer to
// http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for more info.

#if defined(THREAD_SANITIZER)

// Please make sure the code below declares a single string variable
// kTSanDefaultSuppressions contains TSan suppressions delimited by newlines.
// See http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for the instructions on writing suppressions.
char kTSanDefaultSuppressions[] =

// WebRTC specific suppressions.

// False positive in system wrappers.
// https://code.google.com/p/webrtc/issues/detail?id=300
"race:webrtc/system_wrappers/source/thread_posix.cc\n"

// Audio processing
// https://code.google.com/p/webrtc/issues/detail?id=2521 for details.
"race:webrtc/modules/audio_processing/aec/aec_core.c\n"
"race:webrtc/modules/audio_processing/aec/aec_rdft.c\n"

// libjingle_p2p_unittest
// https://code.google.com/p/webrtc/issues/detail?id=2079
"race:webrtc/base/messagequeue.cc\n"
"race:webrtc/base/testclient.cc\n"
"race:webrtc/base/virtualsocketserver.cc\n"
"race:talk/base/messagequeue.cc\n"
"race:talk/base/testclient.cc\n"
"race:talk/base/virtualsocketserver.cc\n"
"race:talk/p2p/base/stunserver_unittest.cc\n"

// libjingle_unittest
// https://code.google.com/p/webrtc/issues/detail?id=2080
"race:webrtc/base/logging.cc\n"
"race:webrtc/base/sharedexclusivelock_unittest.cc\n"
"race:webrtc/base/signalthread_unittest.cc\n"
"race:webrtc/base/thread.cc\n"
"race:talk/base/logging.cc\n"
"race:talk/base/sharedexclusivelock_unittest.cc\n"
"race:talk/base/signalthread_unittest.cc\n"
"race:talk/base/thread.cc\n"

// third_party/usrsctp
// TODO(jiayl): https://code.google.com/p/webrtc/issues/detail?id=3492
"race:third_party/usrsctp/usrsctplib/user_sctp_timer_iterate.c\n"

// Potential deadlocks detected after roll in r6516.
// https://code.google.com/p/webrtc/issues/detail?id=3509
"deadlock:talk/base/criticalsection.h\n"
"deadlock:talk/base/sigslot.h\n"
"deadlock:webrtc/system_wrappers/source/critical_section_posix.cc\n"
"deadlock:webrtc/system_wrappers/source/rw_lock_posix.cc\n"
"deadlock:webrtc/system_wrappers/source/thread_posix.cc\n"



// From Chromium's tsan_suppressions.cc file.

// False positives in libglib.so. Since we don't instrument them, we cannot
// reason about the synchronization in them.
"race:libglib*.so\n"

// Races in libevent, http://crbug.com/23244.
"race:libevent/event.c\n"

// http://crbug.com/84094.
"race:sqlite3StatusSet\n"
"race:pcache1EnforceMaxPage\n"
"race:pcache1AllocPage\n"

// http://crbug.com/157586
"race:third_party/libvpx/source/libvpx/vp8/decoder/threading.c\n"

// http://crbug.com/158922
"race:third_party/libvpx/source/libvpx/vp8/encoder/*\n"

// http://crbug.com/223352
"race:uprv_malloc_46\n"
"race:uprv_realloc_46\n"

// http://crbug.com/244385
"race:unixTempFileDir\n"

// http://crbug.com/244774
"race:webrtc::RTPReceiver::ProcessBitrate\n"
"race:webrtc::RTPSender::ProcessBitrate\n"
"race:webrtc::VideoCodingModuleImpl::Decode\n"
"race:webrtc::RTPSender::SendOutgoingData\n"
"race:webrtc::VP8EncoderImpl::GetEncodedPartitions\n"
"race:webrtc::VP8EncoderImpl::Encode\n"
"race:webrtc::ViEEncoder::DeliverFrame\n"
"race:webrtc::vcm::VideoReceiver::Decode\n"
"race:webrtc::VCMReceiver::FrameForDecoding\n"
"race:*trace_event_unique_catstatic*\n"

// http://crbug.com/244856
"race:AutoPulseLock\n"

// http://crbug.com/246968
"race:webrtc::VideoCodingModuleImpl::RegisterPacketRequestCallback\n"

// http://crbug.com/246970
"race:webrtc::EventPosix::StartTimer\n"

// http://crbug.com/258479
"race:SamplingStateScope\n"
"race:g_trace_state\n"

// http://crbug.com/270037
"race:gLibCleanupFunctions\n"

// http://crbug.com/272987
"race:webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>::set_enabled\n"

// http://crbug.com/345245
"race:jingle_glue::JingleThreadWrapper::~JingleThreadWrapper\n"
"race:webrtc::voe::Channel::UpdatePacketDelay\n"
"race:webrtc::voe::Channel::GetDelayEstimate\n"
"race:webrtc::VCMCodecDataBase::DeregisterReceiveCodec\n"
"race:webrtc::GainControlImpl::set_stream_analog_level\n"

// http://crbug.com/347538
"race:sctp_timer_start\n"

// http://crbug.com/347548
"race:cricket::WebRtcVideoMediaChannel::MaybeResetVieSendCodec\n"
"race:cricket::WebRtcVideoMediaChannel::SetSendCodec\n"

// http://crbug.com/348511
"race:webrtc::acm1::AudioCodingModuleImpl::PlayoutData10Ms\n"

// http://crbug.com/348982
"race:cricket::P2PTransportChannel::OnConnectionDestroyed\n"
"race:cricket::P2PTransportChannel::AddConnection\n"

// http://crbug.com/348984
"race:sctp_express_handle_sack\n"

// http://crbug.com/350982
"race:libvpx/vp9/decoder/vp9_thread.c\n"

// http://crbug.com/372807
"deadlock:net::X509Certificate::CreateCertificateListFromBytes\n"
"deadlock:net::X509Certificate::CreateFromBytes\n"
"deadlock:net::SSLClientSocketNSS::Core::DoHandshakeLoop\n"

// False positive in libc's tzset_internal, http://crbug.com/379738.
"race:tzset_internal\n"

// http://crbug.com/380554
"deadlock:g_type_add_interface_static\n"

// End of suppressions.
;  // Please keep this semicolon.

#endif  // THREAD_SANITIZER
