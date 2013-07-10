//
// libjingle
// Copyright 2004--2007, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "talk/examples/call/mediaenginefactory.h"

#include "talk/base/stringutils.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/filemediaengine.h"
#include "talk/media/base/mediaengine.h"

std::vector<cricket::AudioCodec> RequiredAudioCodecs() {
  std::vector<cricket::AudioCodec> audio_codecs;
  audio_codecs.push_back(
      cricket::AudioCodec(9, "G722", 16000, 0, 1, 0));
  audio_codecs.push_back(
      cricket::AudioCodec(0, "PCMU", 8000, 0, 1, 0));
  audio_codecs.push_back(
      cricket::AudioCodec(13, "CN", 8000, 0, 1, 0));
  audio_codecs.push_back(
      cricket::AudioCodec(105, "CN", 16000, 0, 1, 0));
  return audio_codecs;
}

std::vector<cricket::VideoCodec> RequiredVideoCodecs() {
  std::vector<cricket::VideoCodec> video_codecs;
  video_codecs.push_back(
      cricket::VideoCodec(97, "H264", 320, 240, 30, 0));
  video_codecs.push_back(
      cricket::VideoCodec(99, "H264-SVC", 640, 360, 30, 0));
  return video_codecs;
}

cricket::MediaEngineInterface* MediaEngineFactory::CreateFileMediaEngine(
    const char* voice_in, const char* voice_out,
    const char* video_in, const char* video_out) {
  cricket::FileMediaEngine* file_media_engine = new cricket::FileMediaEngine;
  // Set the RTP dump file names.
  if (voice_in) {
    file_media_engine->set_voice_input_filename(voice_in);
  }
  if (voice_out) {
    file_media_engine->set_voice_output_filename(voice_out);
  }
  if (video_in) {
    file_media_engine->set_video_input_filename(video_in);
  }
  if (video_out) {
    file_media_engine->set_video_output_filename(video_out);
  }

  // Set voice and video codecs. TODO: The codecs actually depend on
  // the the input voice and video streams.
  file_media_engine->set_voice_codecs(RequiredAudioCodecs());
  file_media_engine->set_video_codecs(RequiredVideoCodecs());

  return file_media_engine;
}
