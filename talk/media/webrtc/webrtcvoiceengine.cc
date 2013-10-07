/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_WEBRTC_VOICE

#include "talk/media/webrtc/webrtcvoiceengine.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "talk/base/base64.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/audiorenderer.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/voiceprocessor.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "webrtc/common.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

#ifdef WIN32
#include <objbase.h>  // NOLINT
#endif

namespace cricket {

struct CodecPref {
  const char* name;
  int clockrate;
  int channels;
  int payload_type;
  bool is_multi_rate;
};

static const CodecPref kCodecPrefs[] = {
  { "OPUS",   48000,  2, 111, true },
  { "ISAC",   16000,  1, 103, true },
  { "ISAC",   32000,  1, 104, true },
  { "CELT",   32000,  1, 109, true },
  { "CELT",   32000,  2, 110, true },
  { "G722",   16000,  1, 9,   false },
  { "ILBC",   8000,   1, 102, false },
  { "PCMU",   8000,   1, 0,   false },
  { "PCMA",   8000,   1, 8,   false },
  { "CN",     48000,  1, 107, false },
  { "CN",     32000,  1, 106, false },
  { "CN",     16000,  1, 105, false },
  { "CN",     8000,   1, 13,  false },
  { "red",    8000,   1, 127, false },
  { "telephone-event", 8000, 1, 126, false },
};

// For Linux/Mac, using the default device is done by specifying index 0 for
// VoE 4.0 and not -1 (which was the case for VoE 3.5).
//
// On Windows Vista and newer, Microsoft introduced the concept of "Default
// Communications Device". This means that there are two types of default
// devices (old Wave Audio style default and Default Communications Device).
//
// On Windows systems which only support Wave Audio style default, uses either
// -1 or 0 to select the default device.
//
// On Windows systems which support both "Default Communication Device" and
// old Wave Audio style default, use -1 for Default Communications Device and
// -2 for Wave Audio style default, which is what we want to use for clips.
// It's not clear yet whether the -2 index is handled properly on other OSes.

#ifdef WIN32
static const int kDefaultAudioDeviceId = -1;
static const int kDefaultSoundclipDeviceId = -2;
#else
static const int kDefaultAudioDeviceId = 0;
#endif

// extension header for audio levels, as defined in
// http://tools.ietf.org/html/draft-ietf-avtext-client-to-mixer-audio-level-03
static const char kRtpAudioLevelHeaderExtension[] =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
static const int kRtpAudioLevelHeaderExtensionId = 1;

static const char kIsacCodecName[] = "ISAC";
static const char kL16CodecName[] = "L16";
// Codec parameters for Opus.
static const int kOpusMonoBitrate = 32000;
// Parameter used for NACK.
// This value is equivalent to 5 seconds of audio data at 20 ms per packet.
static const int kNackMaxPackets = 250;
static const int kOpusStereoBitrate = 64000;
// draft-spittka-payload-rtp-opus-03
// Opus bitrate should be in the range between 6000 and 510000.
static const int kOpusMinBitrate = 6000;
static const int kOpusMaxBitrate = 510000;

// Ensure we open the file in a writeable path on ChromeOS and Android. This
// workaround can be removed when it's possible to specify a filename for audio
// option based AEC dumps.
//
// TODO(grunell): Use a string in the options instead of hardcoding it here
// and let the embedder choose the filename (crbug.com/264223).
//
// NOTE(ajm): Don't use hardcoded paths on platforms not explicitly specified
// below.
#if defined(CHROMEOS)
static const char kAecDumpByAudioOptionFilename[] = "/tmp/audio.aecdump";
#elif defined(ANDROID)
static const char kAecDumpByAudioOptionFilename[] = "/sdcard/audio.aecdump";
#else
static const char kAecDumpByAudioOptionFilename[] = "audio.aecdump";
#endif

// Dumps an AudioCodec in RFC 2327-ish format.
static std::string ToString(const AudioCodec& codec) {
  std::stringstream ss;
  ss << codec.name << "/" << codec.clockrate << "/" << codec.channels
     << " (" << codec.id << ")";
  return ss.str();
}
static std::string ToString(const webrtc::CodecInst& codec) {
  std::stringstream ss;
  ss << codec.plname << "/" << codec.plfreq << "/" << codec.channels
     << " (" << codec.pltype << ")";
  return ss.str();
}

static void LogMultiline(talk_base::LoggingSeverity sev, char* text) {
  const char* delim = "\r\n";
  for (char* tok = strtok(text, delim); tok; tok = strtok(NULL, delim)) {
    LOG_V(sev) << tok;
  }
}

// Severity is an integer because it comes is assumed to be from command line.
static int SeverityToFilter(int severity) {
  int filter = webrtc::kTraceNone;
  switch (severity) {
    case talk_base::LS_VERBOSE:
      filter |= webrtc::kTraceAll;
    case talk_base::LS_INFO:
      filter |= (webrtc::kTraceStateInfo | webrtc::kTraceInfo);
    case talk_base::LS_WARNING:
      filter |= (webrtc::kTraceTerseInfo | webrtc::kTraceWarning);
    case talk_base::LS_ERROR:
      filter |= (webrtc::kTraceError | webrtc::kTraceCritical);
  }
  return filter;
}

static bool IsCodecMultiRate(const webrtc::CodecInst& codec) {
  for (size_t i = 0; i < ARRAY_SIZE(kCodecPrefs); ++i) {
    if (_stricmp(kCodecPrefs[i].name, codec.plname) == 0 &&
        kCodecPrefs[i].clockrate == codec.plfreq) {
      return kCodecPrefs[i].is_multi_rate;
    }
  }
  return false;
}

static bool FindCodec(const std::vector<AudioCodec>& codecs,
                      const AudioCodec& codec,
                      AudioCodec* found_codec) {
  for (std::vector<AudioCodec>::const_iterator it = codecs.begin();
       it != codecs.end(); ++it) {
    if (it->Matches(codec)) {
      if (found_codec != NULL) {
        *found_codec = *it;
      }
      return true;
    }
  }
  return false;
}
static bool IsNackEnabled(const AudioCodec& codec) {
  return codec.HasFeedbackParam(FeedbackParam(kRtcpFbParamNack,
                                              kParamValueEmpty));
}


class WebRtcSoundclipMedia : public SoundclipMedia {
 public:
  explicit WebRtcSoundclipMedia(WebRtcVoiceEngine *engine)
      : engine_(engine), webrtc_channel_(-1) {
    engine_->RegisterSoundclip(this);
  }

  virtual ~WebRtcSoundclipMedia() {
    engine_->UnregisterSoundclip(this);
    if (webrtc_channel_ != -1) {
      // We shouldn't have to call Disable() here. DeleteChannel() should call
      // StopPlayout() while deleting the channel.  We should fix the bug
      // inside WebRTC and remove the Disable() call bellow.  This work is
      // tracked by bug http://b/issue?id=5382855.
      PlaySound(NULL, 0, 0);
      Disable();
      if (engine_->voe_sc()->base()->DeleteChannel(webrtc_channel_)
          == -1) {
        LOG_RTCERR1(DeleteChannel, webrtc_channel_);
      }
    }
  }

  bool Init() {
    webrtc_channel_ = engine_->voe_sc()->base()->CreateChannel();
    if (webrtc_channel_ == -1) {
      LOG_RTCERR0(CreateChannel);
      return false;
    }
    return true;
  }

  bool Enable() {
    if (engine_->voe_sc()->base()->StartPlayout(webrtc_channel_) == -1) {
      LOG_RTCERR1(StartPlayout, webrtc_channel_);
      return false;
    }
    return true;
  }

  bool Disable() {
    if (engine_->voe_sc()->base()->StopPlayout(webrtc_channel_) == -1) {
      LOG_RTCERR1(StopPlayout, webrtc_channel_);
      return false;
    }
    return true;
  }

  virtual bool PlaySound(const char *buf, int len, int flags) {
    // The voe file api is not available in chrome.
    if (!engine_->voe_sc()->file()) {
      return false;
    }
    // Must stop playing the current sound (if any), because we are about to
    // modify the stream.
    if (engine_->voe_sc()->file()->StopPlayingFileLocally(webrtc_channel_)
        == -1) {
      LOG_RTCERR1(StopPlayingFileLocally, webrtc_channel_);
      return false;
    }

    if (buf) {
      stream_.reset(new WebRtcSoundclipStream(buf, len));
      stream_->set_loop((flags & SF_LOOP) != 0);
      stream_->Rewind();

      // Play it.
      if (engine_->voe_sc()->file()->StartPlayingFileLocally(
          webrtc_channel_, stream_.get()) == -1) {
        LOG_RTCERR2(StartPlayingFileLocally, webrtc_channel_, stream_.get());
        LOG(LS_ERROR) << "Unable to start soundclip";
        return false;
      }
    } else {
      stream_.reset();
    }
    return true;
  }

  int GetLastEngineError() const { return engine_->voe_sc()->error(); }

 private:
  WebRtcVoiceEngine *engine_;
  int webrtc_channel_;
  talk_base::scoped_ptr<WebRtcSoundclipStream> stream_;
};

WebRtcVoiceEngine::WebRtcVoiceEngine()
    : voe_wrapper_(new VoEWrapper()),
      voe_wrapper_sc_(new VoEWrapper()),
      tracing_(new VoETraceWrapper()),
      adm_(NULL),
      adm_sc_(NULL),
      log_filter_(SeverityToFilter(kDefaultLogSeverity)),
      is_dumping_aec_(false),
      desired_local_monitor_enable_(false),
      tx_processor_ssrc_(0),
      rx_processor_ssrc_(0) {
  Construct();
}

WebRtcVoiceEngine::WebRtcVoiceEngine(VoEWrapper* voe_wrapper,
                                     VoEWrapper* voe_wrapper_sc,
                                     VoETraceWrapper* tracing)
    : voe_wrapper_(voe_wrapper),
      voe_wrapper_sc_(voe_wrapper_sc),
      tracing_(tracing),
      adm_(NULL),
      adm_sc_(NULL),
      log_filter_(SeverityToFilter(kDefaultLogSeverity)),
      is_dumping_aec_(false),
      desired_local_monitor_enable_(false),
      tx_processor_ssrc_(0),
      rx_processor_ssrc_(0) {
  Construct();
}

void WebRtcVoiceEngine::Construct() {
  SetTraceFilter(log_filter_);
  initialized_ = false;
  LOG(LS_VERBOSE) << "WebRtcVoiceEngine::WebRtcVoiceEngine";
  SetTraceOptions("");
  if (tracing_->SetTraceCallback(this) == -1) {
    LOG_RTCERR0(SetTraceCallback);
  }
  if (voe_wrapper_->base()->RegisterVoiceEngineObserver(*this) == -1) {
    LOG_RTCERR0(RegisterVoiceEngineObserver);
  }
  // Clear the default agc state.
  memset(&default_agc_config_, 0, sizeof(default_agc_config_));

  // Load our audio codec list.
  ConstructCodecs();

  // Load our RTP Header extensions.
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpAudioLevelHeaderExtension,
                         kRtpAudioLevelHeaderExtensionId));
}

static bool IsOpus(const AudioCodec& codec) {
  return (_stricmp(codec.name.c_str(), kOpusCodecName) == 0);
}

static bool IsIsac(const AudioCodec& codec) {
  return (_stricmp(codec.name.c_str(), kIsacCodecName) == 0);
}

// True if params["stereo"] == "1"
static bool IsOpusStereoEnabled(const AudioCodec& codec) {
  CodecParameterMap::const_iterator param =
      codec.params.find(kCodecParamStereo);
  if (param == codec.params.end()) {
    return false;
  }
  return param->second == kParamValueTrue;
}

static bool IsValidOpusBitrate(int bitrate) {
  return (bitrate >= kOpusMinBitrate && bitrate <= kOpusMaxBitrate);
}

// Returns 0 if params[kCodecParamMaxAverageBitrate] is not defined or invalid.
// Returns the value of params[kCodecParamMaxAverageBitrate] otherwise.
static int GetOpusBitrateFromParams(const AudioCodec& codec) {
  int bitrate = 0;
  if (!codec.GetParam(kCodecParamMaxAverageBitrate, &bitrate)) {
    return 0;
  }
  if (!IsValidOpusBitrate(bitrate)) {
    LOG(LS_WARNING) << "Codec parameter \"maxaveragebitrate\" has an "
                    << "invalid value: " << bitrate;
    return 0;
  }
  return bitrate;
}

void WebRtcVoiceEngine::ConstructCodecs() {
  LOG(LS_INFO) << "WebRtc VoiceEngine codecs:";
  int ncodecs = voe_wrapper_->codec()->NumOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    webrtc::CodecInst voe_codec;
    if (voe_wrapper_->codec()->GetCodec(i, voe_codec) != -1) {
      // Skip uncompressed formats.
      if (_stricmp(voe_codec.plname, kL16CodecName) == 0) {
        continue;
      }

      const CodecPref* pref = NULL;
      for (size_t j = 0; j < ARRAY_SIZE(kCodecPrefs); ++j) {
        if (_stricmp(kCodecPrefs[j].name, voe_codec.plname) == 0 &&
            kCodecPrefs[j].clockrate == voe_codec.plfreq &&
            kCodecPrefs[j].channels == voe_codec.channels) {
          pref = &kCodecPrefs[j];
          break;
        }
      }

      if (pref) {
        // Use the payload type that we've configured in our pref table;
        // use the offset in our pref table to determine the sort order.
        AudioCodec codec(pref->payload_type, voe_codec.plname, voe_codec.plfreq,
                         voe_codec.rate, voe_codec.channels,
                         ARRAY_SIZE(kCodecPrefs) - (pref - kCodecPrefs));
        LOG(LS_INFO) << ToString(codec);
        if (IsIsac(codec)) {
          // Indicate auto-bandwidth in signaling.
          codec.bitrate = 0;
        }
        if (IsOpus(codec)) {
          // Only add fmtp parameters that differ from the spec.
          if (kPreferredMinPTime != kOpusDefaultMinPTime) {
            codec.params[kCodecParamMinPTime] =
                talk_base::ToString(kPreferredMinPTime);
          }
          if (kPreferredMaxPTime != kOpusDefaultMaxPTime) {
            codec.params[kCodecParamMaxPTime] =
                talk_base::ToString(kPreferredMaxPTime);
          }
          // TODO(hellner): Add ptime, sprop-stereo, stereo and useinbandfec
          // when they can be set to values other than the default.
        }
        codecs_.push_back(codec);
      } else {
        LOG(LS_WARNING) << "Unexpected codec: " << ToString(voe_codec);
      }
    }
  }
  // Make sure they are in local preference order.
  std::sort(codecs_.begin(), codecs_.end(), &AudioCodec::Preferable);
}

WebRtcVoiceEngine::~WebRtcVoiceEngine() {
  LOG(LS_VERBOSE) << "WebRtcVoiceEngine::~WebRtcVoiceEngine";
  if (voe_wrapper_->base()->DeRegisterVoiceEngineObserver() == -1) {
    LOG_RTCERR0(DeRegisterVoiceEngineObserver);
  }
  if (adm_) {
    voe_wrapper_.reset();
    adm_->Release();
    adm_ = NULL;
  }
  if (adm_sc_) {
    voe_wrapper_sc_.reset();
    adm_sc_->Release();
    adm_sc_ = NULL;
  }

  // Test to see if the media processor was deregistered properly
  ASSERT(SignalRxMediaFrame.is_empty());
  ASSERT(SignalTxMediaFrame.is_empty());

  tracing_->SetTraceCallback(NULL);
}

bool WebRtcVoiceEngine::Init(talk_base::Thread* worker_thread) {
  LOG(LS_INFO) << "WebRtcVoiceEngine::Init";
  bool res = InitInternal();
  if (res) {
    LOG(LS_INFO) << "WebRtcVoiceEngine::Init Done!";
  } else {
    LOG(LS_ERROR) << "WebRtcVoiceEngine::Init failed";
    Terminate();
  }
  return res;
}

// Gets the default set of optoins applied to the engine. Historically, these
// were supplied as a combination of flags from the channel manager (ec, agc,
// ns, and highpass) and the rest hardcoded in InitInternal.
static AudioOptions GetDefaultEngineOptions() {
  AudioOptions options;
  options.echo_cancellation.Set(true);
  options.auto_gain_control.Set(true);
  options.noise_suppression.Set(true);
  options.highpass_filter.Set(true);
  options.typing_detection.Set(true);
  options.conference_mode.Set(false);
  options.adjust_agc_delta.Set(0);
  options.experimental_agc.Set(false);
  options.experimental_aec.Set(false);
  options.aec_dump.Set(false);
  return options;
}

bool WebRtcVoiceEngine::InitInternal() {
  // Temporarily turn logging level up for the Init call
  int old_filter = log_filter_;
  int extended_filter = log_filter_ | SeverityToFilter(talk_base::LS_INFO);
  SetTraceFilter(extended_filter);
  SetTraceOptions("");

  // Init WebRtc VoiceEngine.
  if (voe_wrapper_->base()->Init(adm_) == -1) {
    LOG_RTCERR0_EX(Init, voe_wrapper_->error());
    SetTraceFilter(old_filter);
    return false;
  }

  SetTraceFilter(old_filter);
  SetTraceOptions(log_options_);

  // Log the VoiceEngine version info
  char buffer[1024] = "";
  voe_wrapper_->base()->GetVersion(buffer);
  LOG(LS_INFO) << "WebRtc VoiceEngine Version:";
  LogMultiline(talk_base::LS_INFO, buffer);

  // Save the default AGC configuration settings. This must happen before
  // calling SetOptions or the default will be overwritten.
  if (voe_wrapper_->processing()->GetAgcConfig(default_agc_config_) == -1) {
    LOG_RTCERR0(GetAGCConfig);
    return false;
  }

  // Set defaults for options, so that ApplyOptions applies them explicitly
  // when we clear option (channel) overrides. External clients can still
  // modify the defaults via SetOptions (on the media engine).
  if (!SetOptions(GetDefaultEngineOptions())) {
    return false;
  }

  // Print our codec list again for the call diagnostic log
  LOG(LS_INFO) << "WebRtc VoiceEngine codecs:";
  for (std::vector<AudioCodec>::const_iterator it = codecs_.begin();
      it != codecs_.end(); ++it) {
    LOG(LS_INFO) << ToString(*it);
  }

#if defined(LINUX) && !defined(HAVE_LIBPULSE)
  voe_wrapper_sc_->hw()->SetAudioDeviceLayer(webrtc::kAudioLinuxAlsa);
#endif

  // Initialize the VoiceEngine instance that we'll use to play out sound clips.
  if (voe_wrapper_sc_->base()->Init(adm_sc_) == -1) {
    LOG_RTCERR0_EX(Init, voe_wrapper_sc_->error());
    return false;
  }

  // On Windows, tell it to use the default sound (not communication) devices.
  // First check whether there is a valid sound device for playback.
  // TODO(juberti): Clean this up when we support setting the soundclip device.
#ifdef WIN32
  // The SetPlayoutDevice may not be implemented in the case of external ADM.
  // TODO(ronghuawu): We should only check the adm_sc_ here, but current
  // PeerConnection interface never set the adm_sc_, so need to check both
  // in order to determine if the external adm is used.
  if (!adm_ && !adm_sc_) {
    int num_of_devices = 0;
    if (voe_wrapper_sc_->hw()->GetNumOfPlayoutDevices(num_of_devices) != -1 &&
        num_of_devices > 0) {
      if (voe_wrapper_sc_->hw()->SetPlayoutDevice(kDefaultSoundclipDeviceId)
          == -1) {
        LOG_RTCERR1_EX(SetPlayoutDevice, kDefaultSoundclipDeviceId,
                       voe_wrapper_sc_->error());
        return false;
      }
    } else {
      LOG(LS_WARNING) << "No valid sound playout device found.";
    }
  }
#endif

  // Disable the DTMF playout when a tone is sent.
  // PlayDtmfTone will be used if local playout is needed.
  if (voe_wrapper_->dtmf()->SetDtmfFeedbackStatus(false) == -1) {
    LOG_RTCERR1(SetDtmfFeedbackStatus, false);
  }

  initialized_ = true;
  return true;
}

void WebRtcVoiceEngine::Terminate() {
  LOG(LS_INFO) << "WebRtcVoiceEngine::Terminate";
  initialized_ = false;

  StopAecDump();

  voe_wrapper_sc_->base()->Terminate();
  voe_wrapper_->base()->Terminate();
  desired_local_monitor_enable_ = false;
}

int WebRtcVoiceEngine::GetCapabilities() {
  return AUDIO_SEND | AUDIO_RECV;
}

VoiceMediaChannel *WebRtcVoiceEngine::CreateChannel() {
  WebRtcVoiceMediaChannel* ch = new WebRtcVoiceMediaChannel(this);
  if (!ch->valid()) {
    delete ch;
    ch = NULL;
  }
  return ch;
}

SoundclipMedia *WebRtcVoiceEngine::CreateSoundclip() {
  WebRtcSoundclipMedia *soundclip = new WebRtcSoundclipMedia(this);
  if (!soundclip->Init() || !soundclip->Enable()) {
    delete soundclip;
    return NULL;
  }
  return soundclip;
}

bool WebRtcVoiceEngine::SetOptions(const AudioOptions& options) {
  if (!ApplyOptions(options)) {
    return false;
  }
  options_ = options;
  return true;
}

bool WebRtcVoiceEngine::SetOptionOverrides(const AudioOptions& overrides) {
  LOG(LS_INFO) << "Setting option overrides: " << overrides.ToString();
  if (!ApplyOptions(overrides)) {
    return false;
  }
  option_overrides_ = overrides;
  return true;
}

bool WebRtcVoiceEngine::ClearOptionOverrides() {
  LOG(LS_INFO) << "Clearing option overrides.";
  AudioOptions options = options_;
  // Only call ApplyOptions if |options_overrides_| contains overrided options.
  // ApplyOptions affects NS, AGC other options that is shared between
  // all WebRtcVoiceEngineChannels.
  if (option_overrides_ == AudioOptions()) {
    return true;
  }

  if (!ApplyOptions(options)) {
    return false;
  }
  option_overrides_ = AudioOptions();
  return true;
}

// AudioOptions defaults are set in InitInternal (for options with corresponding
// MediaEngineInterface flags) and in SetOptions(int) for flagless options.
bool WebRtcVoiceEngine::ApplyOptions(const AudioOptions& options_in) {
  AudioOptions options = options_in;  // The options are modified below.
  // kEcConference is AEC with high suppression.
  webrtc::EcModes ec_mode = webrtc::kEcConference;
  webrtc::AecmModes aecm_mode = webrtc::kAecmSpeakerphone;
  webrtc::AgcModes agc_mode = webrtc::kAgcAdaptiveAnalog;
  webrtc::NsModes ns_mode = webrtc::kNsHighSuppression;
  bool aecm_comfort_noise = false;

#if defined(IOS)
  // On iOS, VPIO provides built-in EC and AGC.
  options.echo_cancellation.Set(false);
  options.auto_gain_control.Set(false);
#elif defined(ANDROID)
  ec_mode = webrtc::kEcAecm;
#endif

#if defined(IOS) || defined(ANDROID)
  // Set the AGC mode for iOS as well despite disabling it above, to avoid
  // unsupported configuration errors from webrtc.
  agc_mode = webrtc::kAgcFixedDigital;
  options.typing_detection.Set(false);
  options.experimental_agc.Set(false);
  options.experimental_aec.Set(false);
#endif

  LOG(LS_INFO) << "Applying audio options: " << options.ToString();

  webrtc::VoEAudioProcessing* voep = voe_wrapper_->processing();

  bool echo_cancellation;
  if (options.echo_cancellation.Get(&echo_cancellation)) {
    if (voep->SetEcStatus(echo_cancellation, ec_mode) == -1) {
      LOG_RTCERR2(SetEcStatus, echo_cancellation, ec_mode);
      return false;
    }
#if !defined(ANDROID)
    // TODO(ajm): Remove the error return on Android from webrtc.
    if (voep->SetEcMetricsStatus(echo_cancellation) == -1) {
      LOG_RTCERR1(SetEcMetricsStatus, echo_cancellation);
      return false;
    }
#endif
    if (ec_mode == webrtc::kEcAecm) {
      if (voep->SetAecmMode(aecm_mode, aecm_comfort_noise) != 0) {
        LOG_RTCERR2(SetAecmMode, aecm_mode, aecm_comfort_noise);
        return false;
      }
    }
  }

  bool auto_gain_control;
  if (options.auto_gain_control.Get(&auto_gain_control)) {
    if (voep->SetAgcStatus(auto_gain_control, agc_mode) == -1) {
      LOG_RTCERR2(SetAgcStatus, auto_gain_control, agc_mode);
      return false;
    }
  }

  bool noise_suppression;
  if (options.noise_suppression.Get(&noise_suppression)) {
    if (voep->SetNsStatus(noise_suppression, ns_mode) == -1) {
      LOG_RTCERR2(SetNsStatus, noise_suppression, ns_mode);
      return false;
    }
  }

  bool highpass_filter;
  if (options.highpass_filter.Get(&highpass_filter)) {
    if (voep->EnableHighPassFilter(highpass_filter) == -1) {
      LOG_RTCERR1(SetHighpassFilterStatus, highpass_filter);
      return false;
    }
  }

  bool stereo_swapping;
  if (options.stereo_swapping.Get(&stereo_swapping)) {
    voep->EnableStereoChannelSwapping(stereo_swapping);
    if (voep->IsStereoChannelSwappingEnabled() != stereo_swapping) {
      LOG_RTCERR1(EnableStereoChannelSwapping, stereo_swapping);
      return false;
    }
  }

  bool typing_detection;
  if (options.typing_detection.Get(&typing_detection)) {
    if (voep->SetTypingDetectionStatus(typing_detection) == -1) {
      // In case of error, log the info and continue
      LOG_RTCERR1(SetTypingDetectionStatus, typing_detection);
    }
  }

  int adjust_agc_delta;
  if (options.adjust_agc_delta.Get(&adjust_agc_delta)) {
    if (!AdjustAgcLevel(adjust_agc_delta)) {
      return false;
    }
  }

  bool aec_dump;
  if (options.aec_dump.Get(&aec_dump)) {
    if (aec_dump)
      StartAecDump(kAecDumpByAudioOptionFilename);
    else
      StopAecDump();
  }

  bool experimental_aec;
  if (options.experimental_aec.Get(&experimental_aec)) {
    webrtc::AudioProcessing* audioproc =
        voe_wrapper_->base()->audio_processing();
    // We check audioproc for the benefit of tests, since FakeWebRtcVoiceEngine
    // returns NULL on audio_processing().
    if (audioproc) {
      webrtc::Config config;
      config.Set<webrtc::DelayCorrection>(
          new webrtc::DelayCorrection(experimental_aec));
      audioproc->SetExtraOptions(config);
    }
  }


  return true;
}

bool WebRtcVoiceEngine::SetDelayOffset(int offset) {
  voe_wrapper_->processing()->SetDelayOffsetMs(offset);
  if (voe_wrapper_->processing()->DelayOffsetMs() != offset) {
    LOG_RTCERR1(SetDelayOffsetMs, offset);
    return false;
  }

  return true;
}

struct ResumeEntry {
  ResumeEntry(WebRtcVoiceMediaChannel *c, bool p, SendFlags s)
      : channel(c),
        playout(p),
        send(s) {
  }

  WebRtcVoiceMediaChannel *channel;
  bool playout;
  SendFlags send;
};

// TODO(juberti): Refactor this so that the core logic can be used to set the
// soundclip device. At that time, reinstate the soundclip pause/resume code.
bool WebRtcVoiceEngine::SetDevices(const Device* in_device,
                                   const Device* out_device) {
#if !defined(IOS) && !defined(ANDROID)
  int in_id = in_device ? talk_base::FromString<int>(in_device->id) :
      kDefaultAudioDeviceId;
  int out_id = out_device ? talk_base::FromString<int>(out_device->id) :
      kDefaultAudioDeviceId;
  // The device manager uses -1 as the default device, which was the case for
  // VoE 3.5. VoE 4.0, however, uses 0 as the default in Linux and Mac.
#ifndef WIN32
  if (-1 == in_id) {
    in_id = kDefaultAudioDeviceId;
  }
  if (-1 == out_id) {
    out_id = kDefaultAudioDeviceId;
  }
#endif

  std::string in_name = (in_id != kDefaultAudioDeviceId) ?
      in_device->name : "Default device";
  std::string out_name = (out_id != kDefaultAudioDeviceId) ?
      out_device->name : "Default device";
  LOG(LS_INFO) << "Setting microphone to (id=" << in_id << ", name=" << in_name
            << ") and speaker to (id=" << out_id << ", name=" << out_name
            << ")";

  // If we're running the local monitor, we need to stop it first.
  bool ret = true;
  if (!PauseLocalMonitor()) {
    LOG(LS_WARNING) << "Failed to pause local monitor";
    ret = false;
  }

  // Must also pause all audio playback and capture.
  for (ChannelList::const_iterator i = channels_.begin();
       i != channels_.end(); ++i) {
    WebRtcVoiceMediaChannel *channel = *i;
    if (!channel->PausePlayout()) {
      LOG(LS_WARNING) << "Failed to pause playout";
      ret = false;
    }
    if (!channel->PauseSend()) {
      LOG(LS_WARNING) << "Failed to pause send";
      ret = false;
    }
  }

  // Find the recording device id in VoiceEngine and set recording device.
  if (!FindWebRtcAudioDeviceId(true, in_name, in_id, &in_id)) {
    ret = false;
  }
  if (ret) {
    if (voe_wrapper_->hw()->SetRecordingDevice(in_id) == -1) {
      LOG_RTCERR2(SetRecordingDevice, in_device->name, in_id);
      ret = false;
    }
  }

  // Find the playout device id in VoiceEngine and set playout device.
  if (!FindWebRtcAudioDeviceId(false, out_name, out_id, &out_id)) {
    LOG(LS_WARNING) << "Failed to find VoiceEngine device id for " << out_name;
    ret = false;
  }
  if (ret) {
    if (voe_wrapper_->hw()->SetPlayoutDevice(out_id) == -1) {
      LOG_RTCERR2(SetPlayoutDevice, out_device->name, out_id);
      ret = false;
    }
  }

  // Resume all audio playback and capture.
  for (ChannelList::const_iterator i = channels_.begin();
       i != channels_.end(); ++i) {
    WebRtcVoiceMediaChannel *channel = *i;
    if (!channel->ResumePlayout()) {
      LOG(LS_WARNING) << "Failed to resume playout";
      ret = false;
    }
    if (!channel->ResumeSend()) {
      LOG(LS_WARNING) << "Failed to resume send";
      ret = false;
    }
  }

  // Resume local monitor.
  if (!ResumeLocalMonitor()) {
    LOG(LS_WARNING) << "Failed to resume local monitor";
    ret = false;
  }

  if (ret) {
    LOG(LS_INFO) << "Set microphone to (id=" << in_id <<" name=" << in_name
                 << ") and speaker to (id="<< out_id << " name=" << out_name
                 << ")";
  }

  return ret;
#else
  return true;
#endif  // !IOS && !ANDROID
}

bool WebRtcVoiceEngine::FindWebRtcAudioDeviceId(
  bool is_input, const std::string& dev_name, int dev_id, int* rtc_id) {
  // In Linux, VoiceEngine uses the same device dev_id as the device manager.
#ifdef LINUX
  *rtc_id = dev_id;
  return true;
#else
  // In Windows and Mac, we need to find the VoiceEngine device id by name
  // unless the input dev_id is the default device id.
  if (kDefaultAudioDeviceId == dev_id) {
    *rtc_id = dev_id;
    return true;
  }

  // Get the number of VoiceEngine audio devices.
  int count = 0;
  if (is_input) {
    if (-1 == voe_wrapper_->hw()->GetNumOfRecordingDevices(count)) {
      LOG_RTCERR0(GetNumOfRecordingDevices);
      return false;
    }
  } else {
    if (-1 == voe_wrapper_->hw()->GetNumOfPlayoutDevices(count)) {
      LOG_RTCERR0(GetNumOfPlayoutDevices);
      return false;
    }
  }

  for (int i = 0; i < count; ++i) {
    char name[128];
    char guid[128];
    if (is_input) {
      voe_wrapper_->hw()->GetRecordingDeviceName(i, name, guid);
      LOG(LS_VERBOSE) << "VoiceEngine microphone " << i << ": " << name;
    } else {
      voe_wrapper_->hw()->GetPlayoutDeviceName(i, name, guid);
      LOG(LS_VERBOSE) << "VoiceEngine speaker " << i << ": " << name;
    }

    std::string webrtc_name(name);
    if (dev_name.compare(0, webrtc_name.size(), webrtc_name) == 0) {
      *rtc_id = i;
      return true;
    }
  }
  LOG(LS_WARNING) << "VoiceEngine cannot find device: " << dev_name;
  return false;
#endif
}

bool WebRtcVoiceEngine::GetOutputVolume(int* level) {
  unsigned int ulevel;
  if (voe_wrapper_->volume()->GetSpeakerVolume(ulevel) == -1) {
    LOG_RTCERR1(GetSpeakerVolume, level);
    return false;
  }
  *level = ulevel;
  return true;
}

bool WebRtcVoiceEngine::SetOutputVolume(int level) {
  ASSERT(level >= 0 && level <= 255);
  if (voe_wrapper_->volume()->SetSpeakerVolume(level) == -1) {
    LOG_RTCERR1(SetSpeakerVolume, level);
    return false;
  }
  return true;
}

int WebRtcVoiceEngine::GetInputLevel() {
  unsigned int ulevel;
  return (voe_wrapper_->volume()->GetSpeechInputLevel(ulevel) != -1) ?
      static_cast<int>(ulevel) : -1;
}

bool WebRtcVoiceEngine::SetLocalMonitor(bool enable) {
  desired_local_monitor_enable_ = enable;
  return ChangeLocalMonitor(desired_local_monitor_enable_);
}

bool WebRtcVoiceEngine::ChangeLocalMonitor(bool enable) {
  // The voe file api is not available in chrome.
  if (!voe_wrapper_->file()) {
    return false;
  }
  if (enable && !monitor_) {
    monitor_.reset(new WebRtcMonitorStream);
    if (voe_wrapper_->file()->StartRecordingMicrophone(monitor_.get()) == -1) {
      LOG_RTCERR1(StartRecordingMicrophone, monitor_.get());
      // Must call Stop() because there are some cases where Start will report
      // failure but still change the state, and if we leave VE in the on state
      // then it could crash later when trying to invoke methods on our monitor.
      voe_wrapper_->file()->StopRecordingMicrophone();
      monitor_.reset();
      return false;
    }
  } else if (!enable && monitor_) {
    voe_wrapper_->file()->StopRecordingMicrophone();
    monitor_.reset();
  }
  return true;
}

bool WebRtcVoiceEngine::PauseLocalMonitor() {
  return ChangeLocalMonitor(false);
}

bool WebRtcVoiceEngine::ResumeLocalMonitor() {
  return ChangeLocalMonitor(desired_local_monitor_enable_);
}

const std::vector<AudioCodec>& WebRtcVoiceEngine::codecs() {
  return codecs_;
}

bool WebRtcVoiceEngine::FindCodec(const AudioCodec& in) {
  return FindWebRtcCodec(in, NULL);
}

// Get the VoiceEngine codec that matches |in|, with the supplied settings.
bool WebRtcVoiceEngine::FindWebRtcCodec(const AudioCodec& in,
                                        webrtc::CodecInst* out) {
  int ncodecs = voe_wrapper_->codec()->NumOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    webrtc::CodecInst voe_codec;
    if (voe_wrapper_->codec()->GetCodec(i, voe_codec) != -1) {
      AudioCodec codec(voe_codec.pltype, voe_codec.plname, voe_codec.plfreq,
                       voe_codec.rate, voe_codec.channels, 0);
      bool multi_rate = IsCodecMultiRate(voe_codec);
      // Allow arbitrary rates for ISAC to be specified.
      if (multi_rate) {
        // Set codec.bitrate to 0 so the check for codec.Matches() passes.
        codec.bitrate = 0;
      }
      if (codec.Matches(in)) {
        if (out) {
          // Fixup the payload type.
          voe_codec.pltype = in.id;

          // Set bitrate if specified.
          if (multi_rate && in.bitrate != 0) {
            voe_codec.rate = in.bitrate;
          }

          // Apply codec-specific settings.
          if (IsIsac(codec)) {
            // If ISAC and an explicit bitrate is not specified,
            // enable auto bandwidth adjustment.
            voe_codec.rate = (in.bitrate > 0) ? in.bitrate : -1;
          }
          *out = voe_codec;
        }
        return true;
      }
    }
  }
  return false;
}
const std::vector<RtpHeaderExtension>&
WebRtcVoiceEngine::rtp_header_extensions() const {
  return rtp_header_extensions_;
}

void WebRtcVoiceEngine::SetLogging(int min_sev, const char* filter) {
  // if min_sev == -1, we keep the current log level.
  if (min_sev >= 0) {
    SetTraceFilter(SeverityToFilter(min_sev));
  }
  log_options_ = filter;
  SetTraceOptions(initialized_ ? log_options_ : "");
}

int WebRtcVoiceEngine::GetLastEngineError() {
  return voe_wrapper_->error();
}

void WebRtcVoiceEngine::SetTraceFilter(int filter) {
  log_filter_ = filter;
  tracing_->SetTraceFilter(filter);
}

// We suppport three different logging settings for VoiceEngine:
// 1. Observer callback that goes into talk diagnostic logfile.
//    Use --logfile and --loglevel
//
// 2. Encrypted VoiceEngine log for debugging VoiceEngine.
//    Use --voice_loglevel --voice_logfilter "tracefile file_name"
//
// 3. EC log and dump for debugging QualityEngine.
//    Use --voice_loglevel --voice_logfilter "recordEC file_name"
//
// For more details see: "https://sites.google.com/a/google.com/wavelet/Home/
//    Magic-Flute--RTC-Engine-/Magic-Flute-Command-Line-Parameters"
void WebRtcVoiceEngine::SetTraceOptions(const std::string& options) {
  // Set encrypted trace file.
  std::vector<std::string> opts;
  talk_base::tokenize(options, ' ', '"', '"', &opts);
  std::vector<std::string>::iterator tracefile =
      std::find(opts.begin(), opts.end(), "tracefile");
  if (tracefile != opts.end() && ++tracefile != opts.end()) {
    // Write encrypted debug output (at same loglevel) to file
    // EncryptedTraceFile no longer supported.
    if (tracing_->SetTraceFile(tracefile->c_str()) == -1) {
      LOG_RTCERR1(SetTraceFile, *tracefile);
    }
  }

  // Set AEC dump file
  std::vector<std::string>::iterator recordEC =
      std::find(opts.begin(), opts.end(), "recordEC");
  if (recordEC != opts.end()) {
    ++recordEC;
    if (recordEC != opts.end())
      StartAecDump(recordEC->c_str());
    else
      StopAecDump();
  }
}

// Ignore spammy trace messages, mostly from the stats API when we haven't
// gotten RTCP info yet from the remote side.
bool WebRtcVoiceEngine::ShouldIgnoreTrace(const std::string& trace) {
  static const char* kTracesToIgnore[] = {
    "\tfailed to GetReportBlockInformation",
    "GetRecCodec() failed to get received codec",
    "GetReceivedRtcpStatistics: Could not get received RTP statistics",
    "GetRemoteRTCPData() failed to measure statistics due to lack of received RTP and/or RTCP packets",  // NOLINT
    "GetRemoteRTCPData() failed to retrieve sender info for remote side",
    "GetRTPStatistics() failed to measure RTT since no RTP packets have been received yet",  // NOLINT
    "GetRTPStatistics() failed to read RTP statistics from the RTP/RTCP module",
    "GetRTPStatistics() failed to retrieve RTT from the RTP/RTCP module",
    "SenderInfoReceived No received SR",
    "StatisticsRTP() no statistics available",
    "TransmitMixer::TypingDetection() VE_TYPING_NOISE_WARNING message has been posted",  // NOLINT
    "TransmitMixer::TypingDetection() pending noise-saturation warning exists",  // NOLINT
    "GetRecPayloadType() failed to retrieve RX payload type (error=10026)", // NOLINT
    "StopPlayingFileAsMicrophone() isnot playing (error=8088)",
    NULL
  };
  for (const char* const* p = kTracesToIgnore; *p; ++p) {
    if (trace.find(*p) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void WebRtcVoiceEngine::Print(webrtc::TraceLevel level, const char* trace,
                              int length) {
  talk_base::LoggingSeverity sev = talk_base::LS_VERBOSE;
  if (level == webrtc::kTraceError || level == webrtc::kTraceCritical)
    sev = talk_base::LS_ERROR;
  else if (level == webrtc::kTraceWarning)
    sev = talk_base::LS_WARNING;
  else if (level == webrtc::kTraceStateInfo || level == webrtc::kTraceInfo)
    sev = talk_base::LS_INFO;
  else if (level == webrtc::kTraceTerseInfo)
    sev = talk_base::LS_INFO;

  // Skip past boilerplate prefix text
  if (length < 72) {
    std::string msg(trace, length);
    LOG(LS_ERROR) << "Malformed webrtc log message: ";
    LOG_V(sev) << msg;
  } else {
    std::string msg(trace + 71, length - 72);
    if (!ShouldIgnoreTrace(msg)) {
      LOG_V(sev) << "webrtc: " << msg;
    }
  }
}

void WebRtcVoiceEngine::CallbackOnError(int channel_num, int err_code) {
  talk_base::CritScope lock(&channels_cs_);
  WebRtcVoiceMediaChannel* channel = NULL;
  uint32 ssrc = 0;
  LOG(LS_WARNING) << "VoiceEngine error " << err_code << " reported on channel "
                  << channel_num << ".";
  if (FindChannelAndSsrc(channel_num, &channel, &ssrc)) {
    ASSERT(channel != NULL);
    channel->OnError(ssrc, err_code);
  } else {
    LOG(LS_ERROR) << "VoiceEngine channel " << channel_num
                  << " could not be found in channel list when error reported.";
  }
}

bool WebRtcVoiceEngine::FindChannelAndSsrc(
    int channel_num, WebRtcVoiceMediaChannel** channel, uint32* ssrc) const {
  ASSERT(channel != NULL && ssrc != NULL);

  *channel = NULL;
  *ssrc = 0;
  // Find corresponding channel and ssrc
  for (ChannelList::const_iterator it = channels_.begin();
      it != channels_.end(); ++it) {
    ASSERT(*it != NULL);
    if ((*it)->FindSsrc(channel_num, ssrc)) {
      *channel = *it;
      return true;
    }
  }

  return false;
}

// This method will search through the WebRtcVoiceMediaChannels and
// obtain the voice engine's channel number.
bool WebRtcVoiceEngine::FindChannelNumFromSsrc(
    uint32 ssrc, MediaProcessorDirection direction, int* channel_num) {
  ASSERT(channel_num != NULL);
  ASSERT(direction == MPD_RX || direction == MPD_TX);

  *channel_num = -1;
  // Find corresponding channel for ssrc.
  for (ChannelList::const_iterator it = channels_.begin();
      it != channels_.end(); ++it) {
    ASSERT(*it != NULL);
    if (direction & MPD_RX) {
      *channel_num = (*it)->GetReceiveChannelNum(ssrc);
    }
    if (*channel_num == -1 && (direction & MPD_TX)) {
      *channel_num = (*it)->GetSendChannelNum(ssrc);
    }
    if (*channel_num != -1) {
      return true;
    }
  }
  LOG(LS_WARNING) << "FindChannelFromSsrc. No Channel Found for Ssrc: " << ssrc;
  return false;
}

void WebRtcVoiceEngine::RegisterChannel(WebRtcVoiceMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  channels_.push_back(channel);
}

void WebRtcVoiceEngine::UnregisterChannel(WebRtcVoiceMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  ChannelList::iterator i = std::find(channels_.begin(),
                                      channels_.end(),
                                      channel);
  if (i != channels_.end()) {
    channels_.erase(i);
  }
}

void WebRtcVoiceEngine::RegisterSoundclip(WebRtcSoundclipMedia *soundclip) {
  soundclips_.push_back(soundclip);
}

void WebRtcVoiceEngine::UnregisterSoundclip(WebRtcSoundclipMedia *soundclip) {
  SoundclipList::iterator i = std::find(soundclips_.begin(),
                                        soundclips_.end(),
                                        soundclip);
  if (i != soundclips_.end()) {
    soundclips_.erase(i);
  }
}

// Adjusts the default AGC target level by the specified delta.
// NB: If we start messing with other config fields, we'll want
// to save the current webrtc::AgcConfig as well.
bool WebRtcVoiceEngine::AdjustAgcLevel(int delta) {
  webrtc::AgcConfig config = default_agc_config_;
  config.targetLeveldBOv -= delta;

  LOG(LS_INFO) << "Adjusting AGC level from default -"
               << default_agc_config_.targetLeveldBOv << "dB to -"
               << config.targetLeveldBOv << "dB";

  if (voe_wrapper_->processing()->SetAgcConfig(config) == -1) {
    LOG_RTCERR1(SetAgcConfig, config.targetLeveldBOv);
    return false;
  }
  return true;
}

bool WebRtcVoiceEngine::SetAudioDeviceModule(webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc) {
  if (initialized_) {
    LOG(LS_WARNING) << "SetAudioDeviceModule can not be called after Init.";
    return false;
  }
  if (adm_) {
    adm_->Release();
    adm_ = NULL;
  }
  if (adm) {
    adm_ = adm;
    adm_->AddRef();
  }

  if (adm_sc_) {
    adm_sc_->Release();
    adm_sc_ = NULL;
  }
  if (adm_sc) {
    adm_sc_ = adm_sc;
    adm_sc_->AddRef();
  }
  return true;
}

bool WebRtcVoiceEngine::RegisterProcessor(
    uint32 ssrc,
    VoiceProcessor* voice_processor,
    MediaProcessorDirection direction) {
  bool register_with_webrtc = false;
  int channel_id = -1;
  bool success = false;
  uint32* processor_ssrc = NULL;
  bool found_channel = FindChannelNumFromSsrc(ssrc, direction, &channel_id);
  if (voice_processor == NULL || !found_channel) {
    LOG(LS_WARNING) << "Media Processing Registration Failed. ssrc: " << ssrc
        << " foundChannel: " << found_channel;
    return false;
  }

  webrtc::ProcessingTypes processing_type;
  {
    talk_base::CritScope cs(&signal_media_critical_);
    if (direction == MPD_RX) {
      processing_type = webrtc::kPlaybackAllChannelsMixed;
      if (SignalRxMediaFrame.is_empty()) {
        register_with_webrtc = true;
        processor_ssrc = &rx_processor_ssrc_;
      }
      SignalRxMediaFrame.connect(voice_processor,
                                 &VoiceProcessor::OnFrame);
    } else {
      processing_type = webrtc::kRecordingPerChannel;
      if (SignalTxMediaFrame.is_empty()) {
        register_with_webrtc = true;
        processor_ssrc = &tx_processor_ssrc_;
      }
      SignalTxMediaFrame.connect(voice_processor,
                                 &VoiceProcessor::OnFrame);
    }
  }
  if (register_with_webrtc) {
    // TODO(janahan): when registering consider instantiating a
    // a VoeMediaProcess object and not make the engine extend the interface.
    if (voe()->media() && voe()->media()->
        RegisterExternalMediaProcessing(channel_id,
                                        processing_type,
                                        *this) != -1) {
      LOG(LS_INFO) << "Media Processing Registration Succeeded. channel:"
                   << channel_id;
      *processor_ssrc = ssrc;
      success = true;
    } else {
      LOG_RTCERR2(RegisterExternalMediaProcessing,
                  channel_id,
                  processing_type);
      success = false;
    }
  } else {
    // If we don't have to register with the engine, we just needed to
    // connect a new processor, set success to true;
    success = true;
  }
  return success;
}

bool WebRtcVoiceEngine::UnregisterProcessorChannel(
    MediaProcessorDirection channel_direction,
    uint32 ssrc,
    VoiceProcessor* voice_processor,
    MediaProcessorDirection processor_direction) {
  bool success = true;
  FrameSignal* signal;
  webrtc::ProcessingTypes processing_type;
  uint32* processor_ssrc = NULL;
  if (channel_direction == MPD_RX) {
    signal = &SignalRxMediaFrame;
    processing_type = webrtc::kPlaybackAllChannelsMixed;
    processor_ssrc = &rx_processor_ssrc_;
  } else {
    signal = &SignalTxMediaFrame;
    processing_type = webrtc::kRecordingPerChannel;
    processor_ssrc = &tx_processor_ssrc_;
  }

  int deregister_id = -1;
  {
    talk_base::CritScope cs(&signal_media_critical_);
    if ((processor_direction & channel_direction) != 0 && !signal->is_empty()) {
      signal->disconnect(voice_processor);
      int channel_id = -1;
      bool found_channel = FindChannelNumFromSsrc(ssrc,
                                                  channel_direction,
                                                  &channel_id);
      if (signal->is_empty() && found_channel) {
        deregister_id = channel_id;
      }
    }
  }
  if (deregister_id != -1) {
    if (voe()->media() &&
        voe()->media()->DeRegisterExternalMediaProcessing(deregister_id,
        processing_type) != -1) {
      *processor_ssrc = 0;
      LOG(LS_INFO) << "Media Processing DeRegistration Succeeded. channel:"
                   << deregister_id;
    } else {
      LOG_RTCERR2(DeRegisterExternalMediaProcessing,
                  deregister_id,
                  processing_type);
      success = false;
    }
  }
  return success;
}

bool WebRtcVoiceEngine::UnregisterProcessor(
    uint32 ssrc,
    VoiceProcessor* voice_processor,
    MediaProcessorDirection direction) {
  bool success = true;
  if (voice_processor == NULL) {
    LOG(LS_WARNING) << "Media Processing Deregistration Failed. ssrc: "
                    << ssrc;
    return false;
  }
  if (!UnregisterProcessorChannel(MPD_RX, ssrc, voice_processor, direction)) {
    success = false;
  }
  if (!UnregisterProcessorChannel(MPD_TX, ssrc, voice_processor, direction)) {
    success = false;
  }
  return success;
}

// Implementing method from WebRtc VoEMediaProcess interface
// Do not lock mux_channel_cs_ in this callback.
void WebRtcVoiceEngine::Process(int channel,
                                webrtc::ProcessingTypes type,
                                int16_t audio10ms[],
                                int length,
                                int sampling_freq,
                                bool is_stereo) {
    talk_base::CritScope cs(&signal_media_critical_);
    AudioFrame frame(audio10ms, length, sampling_freq, is_stereo);
    if (type == webrtc::kPlaybackAllChannelsMixed) {
      SignalRxMediaFrame(rx_processor_ssrc_, MPD_RX, &frame);
    } else if (type == webrtc::kRecordingPerChannel) {
      SignalTxMediaFrame(tx_processor_ssrc_, MPD_TX, &frame);
    } else {
      LOG(LS_WARNING) << "Media Processing invoked unexpectedly."
                      << " channel: " << channel << " type: " << type
                      << " tx_ssrc: " << tx_processor_ssrc_
                      << " rx_ssrc: " << rx_processor_ssrc_;
    }
}

void WebRtcVoiceEngine::StartAecDump(const std::string& filename) {
  if (!is_dumping_aec_) {
    // Start dumping AEC when we are not dumping.
    if (voe_wrapper_->processing()->StartDebugRecording(
        filename.c_str()) != webrtc::AudioProcessing::kNoError) {
      LOG_RTCERR0(StartDebugRecording);
    } else {
      is_dumping_aec_ = true;
    }
  }
}

void WebRtcVoiceEngine::StopAecDump() {
  if (is_dumping_aec_) {
    // Stop dumping AEC when we are dumping.
    if (voe_wrapper_->processing()->StopDebugRecording() !=
        webrtc::AudioProcessing::kNoError) {
      LOG_RTCERR0(StopDebugRecording);
    }
    is_dumping_aec_ = false;
  }
}

// This struct relies on the generated copy constructor and assignment operator
// since it is used in an stl::map.
struct WebRtcVoiceMediaChannel::WebRtcVoiceChannelInfo {
  WebRtcVoiceChannelInfo() : channel(-1), renderer(NULL) {}
  WebRtcVoiceChannelInfo(int ch, AudioRenderer* r)
      : channel(ch),
        renderer(r) {}
  ~WebRtcVoiceChannelInfo() {}

  int channel;
  AudioRenderer* renderer;
};

// WebRtcVoiceMediaChannel
WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel(WebRtcVoiceEngine *engine)
    : WebRtcMediaChannel<VoiceMediaChannel, WebRtcVoiceEngine>(
          engine,
          engine->voe()->base()->CreateChannel()),
      options_(),
      dtmf_allowed_(false),
      desired_playout_(false),
      nack_enabled_(false),
      playout_(false),
      typing_noise_detected_(false),
      desired_send_(SEND_NOTHING),
      send_(SEND_NOTHING),
      default_receive_ssrc_(0) {
  engine->RegisterChannel(this);
  LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel "
                  << voe_channel();

  ConfigureSendChannel(voe_channel());
}

WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel() {
  LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel "
                  << voe_channel();

  // Remove any remaining send streams, the default channel will be deleted
  // later.
  while (!send_channels_.empty())
    RemoveSendStream(send_channels_.begin()->first);

  // Unregister ourselves from the engine.
  engine()->UnregisterChannel(this);
  // Remove any remaining streams.
  while (!receive_channels_.empty()) {
    RemoveRecvStream(receive_channels_.begin()->first);
  }

  // Delete the default channel.
  DeleteChannel(voe_channel());
}

bool WebRtcVoiceMediaChannel::SetOptions(const AudioOptions& options) {
  LOG(LS_INFO) << "Setting voice channel options: "
               << options.ToString();

  // TODO(xians): Add support to set different options for different send
  // streams after we support multiple APMs.

  // We retain all of the existing options, and apply the given ones
  // on top.  This means there is no way to "clear" options such that
  // they go back to the engine default.
  options_.SetAll(options);

  if (send_ != SEND_NOTHING) {
    if (!engine()->SetOptionOverrides(options_)) {
      LOG(LS_WARNING) <<
          "Failed to engine SetOptionOverrides during channel SetOptions.";
      return false;
    }
  } else {
    // Will be interpreted when appropriate.
  }

  LOG(LS_INFO) << "Set voice channel options.  Current options: "
               << options_.ToString();
  return true;
}

bool WebRtcVoiceMediaChannel::SetRecvCodecs(
    const std::vector<AudioCodec>& codecs) {
  // Set the payload types to be used for incoming media.
  LOG(LS_INFO) << "Setting receive voice codecs:";

  std::vector<AudioCodec> new_codecs;
  // Find all new codecs. We allow adding new codecs but don't allow changing
  // the payload type of codecs that is already configured since we might
  // already be receiving packets with that payload type.
  for (std::vector<AudioCodec>::const_iterator it = codecs.begin();
       it != codecs.end(); ++it) {
    AudioCodec old_codec;
    if (FindCodec(recv_codecs_, *it, &old_codec)) {
      if (old_codec.id != it->id) {
        LOG(LS_ERROR) << it->name << " payload type changed.";
        return false;
      }
    } else {
      new_codecs.push_back(*it);
    }
  }
  if (new_codecs.empty()) {
    // There are no new codecs to configure. Already configured codecs are
    // never removed.
    return true;
  }

  if (playout_) {
    // Receive codecs can not be changed while playing. So we temporarily
    // pause playout.
    PausePlayout();
  }

  bool ret = true;
  for (std::vector<AudioCodec>::const_iterator it = new_codecs.begin();
       it != new_codecs.end() && ret; ++it) {
    webrtc::CodecInst voe_codec;
    if (engine()->FindWebRtcCodec(*it, &voe_codec)) {
      LOG(LS_INFO) << ToString(*it);
      voe_codec.pltype = it->id;
      if (default_receive_ssrc_ == 0) {
        // Set the receive codecs on the default channel explicitly if the
        // default channel is not used by |receive_channels_|, this happens in
        // conference mode or in non-conference mode when there is no playout
        // channel.
        // TODO(xians): Figure out how we use the default channel in conference
        // mode.
        if (engine()->voe()->codec()->SetRecPayloadType(
            voe_channel(), voe_codec) == -1) {
          LOG_RTCERR2(SetRecPayloadType, voe_channel(), ToString(voe_codec));
          ret = false;
        }
      }

      // Set the receive codecs on all receiving channels.
      for (ChannelMap::iterator it = receive_channels_.begin();
           it != receive_channels_.end() && ret; ++it) {
        if (engine()->voe()->codec()->SetRecPayloadType(
                it->second.channel, voe_codec) == -1) {
          LOG_RTCERR2(SetRecPayloadType, it->second.channel,
                      ToString(voe_codec));
          ret = false;
        }
      }
    } else {
      LOG(LS_WARNING) << "Unknown codec " << ToString(*it);
      ret = false;
    }
  }
  if (ret) {
    recv_codecs_ = codecs;
  }

  if (desired_playout_ && !playout_) {
    ResumePlayout();
  }
  return ret;
}

bool WebRtcVoiceMediaChannel::SetSendCodecs(
    int channel, const std::vector<AudioCodec>& codecs) {
  // Disable VAD, and FEC unless we know the other side wants them.
  engine()->voe()->codec()->SetVADStatus(channel, false);
  engine()->voe()->rtp()->SetNACKStatus(channel, false, 0);
  engine()->voe()->rtp()->SetFECStatus(channel, false);

  // Scan through the list to figure out the codec to use for sending, along
  // with the proper configuration for VAD and DTMF.
  bool first = true;
  webrtc::CodecInst send_codec;
  memset(&send_codec, 0, sizeof(send_codec));

  for (std::vector<AudioCodec>::const_iterator it = codecs.begin();
       it != codecs.end(); ++it) {
    // Ignore codecs we don't know about. The negotiation step should prevent
    // this, but double-check to be sure.
    webrtc::CodecInst voe_codec;
    if (!engine()->FindWebRtcCodec(*it, &voe_codec)) {
      LOG(LS_WARNING) << "Unknown codec " << ToString(voe_codec);
      continue;
    }

    // If OPUS, change what we send according to the "stereo" codec
    // parameter, and not the "channels" parameter.  We set
    // voe_codec.channels to 2 if "stereo=1" and 1 otherwise.  If
    // the bitrate is not specified, i.e. is zero, we set it to the
    // appropriate default value for mono or stereo Opus.
    if (IsOpus(*it)) {
      if (IsOpusStereoEnabled(*it)) {
        voe_codec.channels = 2;
        if (!IsValidOpusBitrate(it->bitrate)) {
          if (it->bitrate != 0) {
            LOG(LS_WARNING) << "Overrides the invalid supplied bitrate("
                            << it->bitrate
                            << ") with default opus stereo bitrate: "
                            << kOpusStereoBitrate;
          }
          voe_codec.rate = kOpusStereoBitrate;
        }
      } else {
        voe_codec.channels = 1;
        if (!IsValidOpusBitrate(it->bitrate)) {
          if (it->bitrate != 0) {
            LOG(LS_WARNING) << "Overrides the invalid supplied bitrate("
                            << it->bitrate
                            << ") with default opus mono bitrate: "
                            << kOpusMonoBitrate;
          }
          voe_codec.rate = kOpusMonoBitrate;
        }
      }
      int bitrate_from_params = GetOpusBitrateFromParams(*it);
      if (bitrate_from_params != 0) {
        voe_codec.rate = bitrate_from_params;
      }
    }

    // Find the DTMF telephone event "codec" and tell VoiceEngine channels
    // about it.
    if (_stricmp(it->name.c_str(), "telephone-event") == 0 ||
        _stricmp(it->name.c_str(), "audio/telephone-event") == 0) {
      if (engine()->voe()->dtmf()->SetSendTelephoneEventPayloadType(
              channel, it->id) == -1) {
        LOG_RTCERR2(SetSendTelephoneEventPayloadType, channel, it->id);
        return false;
      }
    }

    // Turn voice activity detection/comfort noise on if supported.
    // Set the wideband CN payload type appropriately.
    // (narrowband always uses the static payload type 13).
    if (_stricmp(it->name.c_str(), "CN") == 0) {
      webrtc::PayloadFrequencies cn_freq;
      switch (it->clockrate) {
        case 8000:
          cn_freq = webrtc::kFreq8000Hz;
          break;
        case 16000:
          cn_freq = webrtc::kFreq16000Hz;
          break;
        case 32000:
          cn_freq = webrtc::kFreq32000Hz;
          break;
        default:
          LOG(LS_WARNING) << "CN frequency " << it->clockrate
                          << " not supported.";
          continue;
      }
      // Set the CN payloadtype and the VAD status.
      // The CN payload type for 8000 Hz clockrate is fixed at 13.
      if (cn_freq != webrtc::kFreq8000Hz) {
        if (engine()->voe()->codec()->SetSendCNPayloadType(
                channel, it->id, cn_freq) == -1) {
          LOG_RTCERR3(SetSendCNPayloadType, channel, it->id, cn_freq);
          // TODO(ajm): This failure condition will be removed from VoE.
          // Restore the return here when we update to a new enough webrtc.
          //
          // Not returning false because the SetSendCNPayloadType will fail if
          // the channel is already sending.
          // This can happen if the remote description is applied twice, for
          // example in the case of ROAP on top of JSEP, where both side will
          // send the offer.
        }
      }

      // Only turn on VAD if we have a CN payload type that matches the
      // clockrate for the codec we are going to use.
      if (it->clockrate == send_codec.plfreq) {
        LOG(LS_INFO) << "Enabling VAD";
        if (engine()->voe()->codec()->SetVADStatus(channel, true) == -1) {
          LOG_RTCERR2(SetVADStatus, channel, true);
          return false;
        }
      }
    }

    // We'll use the first codec in the list to actually send audio data.
    // Be sure to use the payload type requested by the remote side.
    // "red", for FEC audio, is a special case where the actual codec to be
    // used is specified in params.
    if (first) {
      if (_stricmp(it->name.c_str(), "red") == 0) {
        // Parse out the RED parameters. If we fail, just ignore RED;
        // we don't support all possible params/usage scenarios.
        if (!GetRedSendCodec(*it, codecs, &send_codec)) {
          continue;
        }

        // Enable redundant encoding of the specified codec. Treat any
        // failure as a fatal internal error.
        LOG(LS_INFO) << "Enabling FEC";
        if (engine()->voe()->rtp()->SetFECStatus(channel, true, it->id) == -1) {
          LOG_RTCERR3(SetFECStatus, channel, true, it->id);
          return false;
        }
      } else {
        send_codec = voe_codec;
        nack_enabled_ = IsNackEnabled(*it);
        SetNack(channel, nack_enabled_);
      }
      first = false;
      // Set the codec immediately, since SetVADStatus() depends on whether
      // the current codec is mono or stereo.
      if (!SetSendCodec(channel, send_codec))
        return false;
    }
  }

  // If we're being asked to set an empty list of codecs, due to a buggy client,
  // choose the most common format: PCMU
  if (first) {
    LOG(LS_WARNING) << "Received empty list of codecs; using PCMU/8000";
    AudioCodec codec(0, "PCMU", 8000, 0, 1, 0);
    engine()->FindWebRtcCodec(codec, &send_codec);
    if (!SetSendCodec(channel, send_codec))
      return false;
  }

  // Always update the |send_codec_| to the currently set send codec.
  send_codec_.reset(new webrtc::CodecInst(send_codec));

  return true;
}

bool WebRtcVoiceMediaChannel::SetSendCodecs(
    const std::vector<AudioCodec>& codecs) {
  dtmf_allowed_ = false;
  for (std::vector<AudioCodec>::const_iterator it = codecs.begin();
       it != codecs.end(); ++it) {
    // Find the DTMF telephone event "codec".
    if (_stricmp(it->name.c_str(), "telephone-event") == 0 ||
        _stricmp(it->name.c_str(), "audio/telephone-event") == 0) {
      dtmf_allowed_ = true;
    }
  }

  // Cache the codecs in order to configure the channel created later.
  send_codecs_ = codecs;
  for (ChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    if (!SetSendCodecs(iter->second.channel, codecs)) {
      return false;
    }
  }

  SetNack(receive_channels_, nack_enabled_);

  return true;
}

void WebRtcVoiceMediaChannel::SetNack(const ChannelMap& channels,
                                      bool nack_enabled) {
  for (ChannelMap::const_iterator it = channels.begin();
       it != channels.end(); ++it) {
    SetNack(it->second.channel, nack_enabled);
  }
}

void WebRtcVoiceMediaChannel::SetNack(int channel, bool nack_enabled) {
  if (nack_enabled) {
    LOG(LS_INFO) << "Enabling NACK for channel " << channel;
    engine()->voe()->rtp()->SetNACKStatus(channel, true, kNackMaxPackets);
  } else {
    LOG(LS_INFO) << "Disabling NACK for channel " << channel;
    engine()->voe()->rtp()->SetNACKStatus(channel, false, 0);
  }
}

bool WebRtcVoiceMediaChannel::SetSendCodec(
    const webrtc::CodecInst& send_codec) {
  LOG(LS_INFO) << "Selected voice codec " << ToString(send_codec)
               << ", bitrate=" << send_codec.rate;
  for (ChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    if (!SetSendCodec(iter->second.channel, send_codec))
      return false;
  }

  return true;
}

bool WebRtcVoiceMediaChannel::SetSendCodec(
    int channel, const webrtc::CodecInst& send_codec) {
  LOG(LS_INFO) << "Send channel " << channel <<  " selected voice codec "
               << ToString(send_codec) << ", bitrate=" << send_codec.rate;

  if (engine()->voe()->codec()->SetSendCodec(channel, send_codec) == -1) {
    LOG_RTCERR2(SetSendCodec, channel, ToString(send_codec));
    return false;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::SetRecvRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  // We don't support any incoming extensions headers right now.
  return true;
}

bool WebRtcVoiceMediaChannel::SetSendRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  // Enable the audio level extension header if requested.
  std::vector<RtpHeaderExtension>::const_iterator it;
  for (it = extensions.begin(); it != extensions.end(); ++it) {
    if (it->uri == kRtpAudioLevelHeaderExtension) {
      break;
    }
  }

  bool enable = (it != extensions.end());
  int id = 0;

  if (enable) {
    id = it->id;
    if (id < kMinRtpHeaderExtensionId ||
        id > kMaxRtpHeaderExtensionId) {
      LOG(LS_WARNING) << "Invalid RTP header extension id " << id;
      return false;
    }
  }

  LOG(LS_INFO) << "Enabling audio level header extension with ID " << id;
  for (ChannelMap::const_iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    if (engine()->voe()->rtp()->SetRTPAudioLevelIndicationStatus(
            iter->second.channel, enable, id) == -1) {
      LOG_RTCERR3(SetRTPAudioLevelIndicationStatus,
                  iter->second.channel, enable, id);
      return false;
    }
  }

  return true;
}

bool WebRtcVoiceMediaChannel::SetPlayout(bool playout) {
  desired_playout_ = playout;
  return ChangePlayout(desired_playout_);
}

bool WebRtcVoiceMediaChannel::PausePlayout() {
  return ChangePlayout(false);
}

bool WebRtcVoiceMediaChannel::ResumePlayout() {
  return ChangePlayout(desired_playout_);
}

bool WebRtcVoiceMediaChannel::ChangePlayout(bool playout) {
  if (playout_ == playout) {
    return true;
  }

  // Change the playout of all channels to the new state.
  bool result = true;
  if (receive_channels_.empty()) {
    // Only toggle the default channel if we don't have any other channels.
    result = SetPlayout(voe_channel(), playout);
  }
  for (ChannelMap::iterator it = receive_channels_.begin();
       it != receive_channels_.end() && result; ++it) {
    if (!SetPlayout(it->second.channel, playout)) {
      LOG(LS_ERROR) << "SetPlayout " << playout << " on channel "
                    << it->second.channel << " failed";
      result = false;
    }
  }

  if (result) {
    playout_ = playout;
  }
  return result;
}

bool WebRtcVoiceMediaChannel::SetSend(SendFlags send) {
  desired_send_ = send;
  if (!send_channels_.empty())
    return ChangeSend(desired_send_);
  return true;
}

bool WebRtcVoiceMediaChannel::PauseSend() {
  return ChangeSend(SEND_NOTHING);
}

bool WebRtcVoiceMediaChannel::ResumeSend() {
  return ChangeSend(desired_send_);
}

bool WebRtcVoiceMediaChannel::ChangeSend(SendFlags send) {
  if (send_ == send) {
    return true;
  }

  // Change the settings on each send channel.
  if (send == SEND_MICROPHONE)
    engine()->SetOptionOverrides(options_);

  // Change the settings on each send channel.
  for (ChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    if (!ChangeSend(iter->second.channel, send))
      return false;
  }

  // Clear up the options after stopping sending.
  if (send == SEND_NOTHING)
    engine()->ClearOptionOverrides();

  send_ = send;
  return true;
}

bool WebRtcVoiceMediaChannel::ChangeSend(int channel, SendFlags send) {
  if (send == SEND_MICROPHONE) {
    if (engine()->voe()->base()->StartSend(channel) == -1) {
      LOG_RTCERR1(StartSend, channel);
      return false;
    }
    if (engine()->voe()->file() &&
        engine()->voe()->file()->StopPlayingFileAsMicrophone(channel) == -1) {
      LOG_RTCERR1(StopPlayingFileAsMicrophone, channel);
      return false;
    }
  } else {  // SEND_NOTHING
    ASSERT(send == SEND_NOTHING);
    if (engine()->voe()->base()->StopSend(channel) == -1) {
      LOG_RTCERR1(StopSend, channel);
      return false;
    }
  }

  return true;
}

void WebRtcVoiceMediaChannel::ConfigureSendChannel(int channel) {
  if (engine()->voe()->network()->RegisterExternalTransport(
          channel, *this) == -1) {
    LOG_RTCERR2(RegisterExternalTransport, channel, this);
  }

  // Enable RTCP (for quality stats and feedback messages)
  EnableRtcp(channel);

  // Reset all recv codecs; they will be enabled via SetRecvCodecs.
  ResetRecvCodecs(channel);
}

bool WebRtcVoiceMediaChannel::DeleteChannel(int channel) {
  if (engine()->voe()->network()->DeRegisterExternalTransport(channel) == -1) {
    LOG_RTCERR1(DeRegisterExternalTransport, channel);
  }

  if (engine()->voe()->base()->DeleteChannel(channel) == -1) {
    LOG_RTCERR1(DeleteChannel, channel);
    return false;
  }

  return true;
}

bool WebRtcVoiceMediaChannel::AddSendStream(const StreamParams& sp) {
  // If the default channel is already used for sending create a new channel
  // otherwise use the default channel for sending.
  int channel = GetSendChannelNum(sp.first_ssrc());
  if (channel != -1) {
    LOG(LS_ERROR) << "Stream already exists with ssrc " << sp.first_ssrc();
    return false;
  }

  bool default_channel_is_available = true;
  for (ChannelMap::const_iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    if (IsDefaultChannel(iter->second.channel)) {
      default_channel_is_available = false;
      break;
    }
  }
  if (default_channel_is_available) {
    channel = voe_channel();
  } else {
    // Create a new channel for sending audio data.
    channel = engine()->voe()->base()->CreateChannel();
    if (channel == -1) {
      LOG_RTCERR0(CreateChannel);
      return false;
    }

    ConfigureSendChannel(channel);
  }

  // Save the channel to send_channels_, so that RemoveSendStream() can still
  // delete the channel in case failure happens below.
  send_channels_[sp.first_ssrc()] = WebRtcVoiceChannelInfo(channel, NULL);

  // Set the send (local) SSRC.
  // If there are multiple send SSRCs, we can only set the first one here, and
  // the rest of the SSRC(s) need to be set after SetSendCodec has been called
  // (with a codec requires multiple SSRC(s)).
  if (engine()->voe()->rtp()->SetLocalSSRC(channel, sp.first_ssrc()) == -1) {
    LOG_RTCERR2(SetSendSSRC, channel, sp.first_ssrc());
    return false;
  }

  // At this point the channel's local SSRC has been updated. If the channel is
  // the default channel make sure that all the receive channels are updated as
  // well. Receive channels have to have the same SSRC as the default channel in
  // order to send receiver reports with this SSRC.
  if (IsDefaultChannel(channel)) {
    for (ChannelMap::const_iterator it = receive_channels_.begin();
         it != receive_channels_.end(); ++it) {
      // Only update the SSRC for non-default channels.
      if (!IsDefaultChannel(it->second.channel)) {
        if (engine()->voe()->rtp()->SetLocalSSRC(it->second.channel,
                                                 sp.first_ssrc()) != 0) {
          LOG_RTCERR2(SetLocalSSRC, it->second.channel, sp.first_ssrc());
          return false;
        }
      }
    }
  }

  if (engine()->voe()->rtp()->SetRTCP_CNAME(channel, sp.cname.c_str()) == -1) {
     LOG_RTCERR2(SetRTCP_CNAME, channel, sp.cname);
     return false;
  }

  // Set the current codecs to be used for the new channel.
  if (!send_codecs_.empty() && !SetSendCodecs(channel, send_codecs_))
    return false;

  return ChangeSend(channel, desired_send_);
}

bool WebRtcVoiceMediaChannel::RemoveSendStream(uint32 ssrc) {
  ChannelMap::iterator it = send_channels_.find(ssrc);
  if (it == send_channels_.end()) {
    LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                    << " which doesn't exist.";
    return false;
  }

  int channel = it->second.channel;
  ChangeSend(channel, SEND_NOTHING);

  // Notify the audio renderer that the send channel is going away.
  if (it->second.renderer)
    it->second.renderer->RemoveChannel(channel);

  if (IsDefaultChannel(channel)) {
    // Do not delete the default channel since the receive channels depend on
    // the default channel, recycle it instead.
    ChangeSend(channel, SEND_NOTHING);
  } else {
    // Clean up and delete the send channel.
    LOG(LS_INFO) << "Removing audio send stream " << ssrc
                 << " with VoiceEngine channel #" << channel << ".";
    if (!DeleteChannel(channel))
      return false;
  }

  send_channels_.erase(it);
  if (send_channels_.empty())
    ChangeSend(SEND_NOTHING);

  return true;
}

bool WebRtcVoiceMediaChannel::AddRecvStream(const StreamParams& sp) {
  talk_base::CritScope lock(&receive_channels_cs_);

  if (!VERIFY(sp.ssrcs.size() == 1))
    return false;
  uint32 ssrc = sp.first_ssrc();

  if (ssrc == 0) {
    LOG(LS_WARNING) << "AddRecvStream with 0 ssrc is not supported.";
    return false;
  }

  if (receive_channels_.find(ssrc) != receive_channels_.end()) {
    LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  // Reuse default channel for recv stream in non-conference mode call
  // when the default channel is not being used.
  if (!InConferenceMode() && default_receive_ssrc_ == 0) {
    LOG(LS_INFO) << "Recv stream " << sp.first_ssrc()
                 << " reuse default channel";
    default_receive_ssrc_ = sp.first_ssrc();
    receive_channels_.insert(std::make_pair(
        default_receive_ssrc_, WebRtcVoiceChannelInfo(voe_channel(), NULL)));
    return SetPlayout(voe_channel(), playout_);
  }

  // Create a new channel for receiving audio data.
  int channel = engine()->voe()->base()->CreateChannel();
  if (channel == -1) {
    LOG_RTCERR0(CreateChannel);
    return false;
  }

  if (!ConfigureRecvChannel(channel)) {
    DeleteChannel(channel);
    return false;
  }

  receive_channels_.insert(
      std::make_pair(ssrc, WebRtcVoiceChannelInfo(channel, NULL)));

  LOG(LS_INFO) << "New audio stream " << ssrc
               << " registered to VoiceEngine channel #"
               << channel << ".";
  return true;
}

bool WebRtcVoiceMediaChannel::ConfigureRecvChannel(int channel) {
  // Configure to use external transport, like our default channel.
  if (engine()->voe()->network()->RegisterExternalTransport(
          channel, *this) == -1) {
    LOG_RTCERR2(SetExternalTransport, channel, this);
    return false;
  }

  // Use the same SSRC as our default channel (so the RTCP reports are correct).
  unsigned int send_ssrc;
  webrtc::VoERTP_RTCP* rtp = engine()->voe()->rtp();
  if (rtp->GetLocalSSRC(voe_channel(), send_ssrc) == -1) {
    LOG_RTCERR2(GetSendSSRC, channel, send_ssrc);
    return false;
  }
  if (rtp->SetLocalSSRC(channel, send_ssrc) == -1) {
    LOG_RTCERR2(SetSendSSRC, channel, send_ssrc);
    return false;
  }

  // Use the same recv payload types as our default channel.
  ResetRecvCodecs(channel);
  if (!recv_codecs_.empty()) {
    for (std::vector<AudioCodec>::const_iterator it = recv_codecs_.begin();
        it != recv_codecs_.end(); ++it) {
      webrtc::CodecInst voe_codec;
      if (engine()->FindWebRtcCodec(*it, &voe_codec)) {
        voe_codec.pltype = it->id;
        voe_codec.rate = 0;  // Needed to make GetRecPayloadType work for ISAC
        if (engine()->voe()->codec()->GetRecPayloadType(
            voe_channel(), voe_codec) != -1) {
          if (engine()->voe()->codec()->SetRecPayloadType(
              channel, voe_codec) == -1) {
            LOG_RTCERR2(SetRecPayloadType, channel, ToString(voe_codec));
            return false;
          }
        }
      }
    }
  }

  if (InConferenceMode()) {
    // To be in par with the video, voe_channel() is not used for receiving in
    // a conference call.
    if (receive_channels_.empty() && default_receive_ssrc_ == 0 && playout_) {
      // This is the first stream in a multi user meeting. We can now
      // disable playback of the default stream. This since the default
      // stream will probably have received some initial packets before
      // the new stream was added. This will mean that the CN state from
      // the default channel will be mixed in with the other streams
      // throughout the whole meeting, which might be disturbing.
      LOG(LS_INFO) << "Disabling playback on the default voice channel";
      SetPlayout(voe_channel(), false);
    }
  }
  SetNack(channel, nack_enabled_);

  return SetPlayout(channel, playout_);
}

bool WebRtcVoiceMediaChannel::RemoveRecvStream(uint32 ssrc) {
  talk_base::CritScope lock(&receive_channels_cs_);
  ChannelMap::iterator it = receive_channels_.find(ssrc);
  if (it == receive_channels_.end()) {
    LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                    << " which doesn't exist.";
    return false;
  }

  if (ssrc == default_receive_ssrc_) {
    ASSERT(IsDefaultChannel(it->second.channel));
    // Recycle the default channel is for recv stream.
    if (playout_)
      SetPlayout(voe_channel(), false);

    if (it->second.renderer)
      it->second.renderer->RemoveChannel(voe_channel());

    default_receive_ssrc_ = 0;
    receive_channels_.erase(it);
    return true;
  }

  // Non default channel.
  // Notify the renderer that channel is going away.
  if (it->second.renderer)
    it->second.renderer->RemoveChannel(it->second.channel);

  LOG(LS_INFO) << "Removing audio stream " << ssrc
               << " with VoiceEngine channel #" << it->second.channel << ".";
  if (!DeleteChannel(it->second.channel)) {
    // Erase the entry anyhow.
    receive_channels_.erase(it);
    return false;
  }

  receive_channels_.erase(it);
  bool enable_default_channel_playout = false;
  if (receive_channels_.empty()) {
    // The last stream was removed. We can now enable the default
    // channel for new channels to be played out immediately without
    // waiting for AddStream messages.
    // We do this for both conference mode and non-conference mode.
    // TODO(oja): Does the default channel still have it's CN state?
    enable_default_channel_playout = true;
  }
  if (!InConferenceMode() && receive_channels_.size() == 1 &&
      default_receive_ssrc_ != 0) {
    // Only the default channel is active, enable the playout on default
    // channel.
    enable_default_channel_playout = true;
  }
  if (enable_default_channel_playout && playout_) {
    LOG(LS_INFO) << "Enabling playback on the default voice channel";
    SetPlayout(voe_channel(), true);
  }

  return true;
}

bool WebRtcVoiceMediaChannel::SetRemoteRenderer(uint32 ssrc,
                                                AudioRenderer* renderer) {
  ChannelMap::iterator it = receive_channels_.find(ssrc);
  if (it == receive_channels_.end()) {
    if (renderer) {
      // Return an error if trying to set a valid renderer with an invalid ssrc.
      LOG(LS_ERROR) << "SetRemoteRenderer failed with ssrc "<< ssrc;
      return false;
    }

    // The channel likely has gone away, do nothing.
    return true;
  }

  AudioRenderer* remote_renderer = it->second.renderer;
  if (renderer) {
    ASSERT(remote_renderer == NULL || remote_renderer == renderer);
    if (!remote_renderer) {
      renderer->AddChannel(it->second.channel);
    }
  } else if (remote_renderer) {
    // |renderer| == NULL, remove the channel from the renderer.
    remote_renderer->RemoveChannel(it->second.channel);
  }

  // Assign the new value to the struct.
  it->second.renderer = renderer;
  return true;
}

bool WebRtcVoiceMediaChannel::SetLocalRenderer(uint32 ssrc,
                                               AudioRenderer* renderer) {
  ChannelMap::iterator it = send_channels_.find(ssrc);
  if (it == send_channels_.end()) {
    if (renderer) {
      // Return an error if trying to set a valid renderer with an invalid ssrc.
      LOG(LS_ERROR) << "SetLocalRenderer failed with ssrc "<< ssrc;
      return false;
    }

    // The channel likely has gone away, do nothing.
    return true;
  }

  AudioRenderer* local_renderer = it->second.renderer;
  if (renderer) {
    ASSERT(local_renderer == NULL || local_renderer == renderer);
    if (!local_renderer)
      renderer->AddChannel(it->second.channel);
  } else if (local_renderer) {
    local_renderer->RemoveChannel(it->second.channel);
  }

  // Assign the new value to the struct.
  it->second.renderer = renderer;
  return true;
}

bool WebRtcVoiceMediaChannel::GetActiveStreams(
    AudioInfo::StreamList* actives) {
  // In conference mode, the default channel should not be in
  // |receive_channels_|.
  actives->clear();
  for (ChannelMap::iterator it = receive_channels_.begin();
       it != receive_channels_.end(); ++it) {
    int level = GetOutputLevel(it->second.channel);
    if (level > 0) {
      actives->push_back(std::make_pair(it->first, level));
    }
  }
  return true;
}

int WebRtcVoiceMediaChannel::GetOutputLevel() {
  // return the highest output level of all streams
  int highest = GetOutputLevel(voe_channel());
  for (ChannelMap::iterator it = receive_channels_.begin();
       it != receive_channels_.end(); ++it) {
    int level = GetOutputLevel(it->second.channel);
    highest = talk_base::_max(level, highest);
  }
  return highest;
}

int WebRtcVoiceMediaChannel::GetTimeSinceLastTyping() {
  int ret;
  if (engine()->voe()->processing()->TimeSinceLastTyping(ret) == -1) {
    // In case of error, log the info and continue
    LOG_RTCERR0(TimeSinceLastTyping);
    ret = -1;
  } else {
    ret *= 1000;  // We return ms, webrtc returns seconds.
  }
  return ret;
}

void WebRtcVoiceMediaChannel::SetTypingDetectionParameters(int time_window,
    int cost_per_typing, int reporting_threshold, int penalty_decay,
    int type_event_delay) {
  if (engine()->voe()->processing()->SetTypingDetectionParameters(
          time_window, cost_per_typing,
          reporting_threshold, penalty_decay, type_event_delay) == -1) {
    // In case of error, log the info and continue
    LOG_RTCERR5(SetTypingDetectionParameters, time_window,
                cost_per_typing, reporting_threshold, penalty_decay,
                type_event_delay);
  }
}

bool WebRtcVoiceMediaChannel::SetOutputScaling(
    uint32 ssrc, double left, double right) {
  talk_base::CritScope lock(&receive_channels_cs_);
  // Collect the channels to scale the output volume.
  std::vector<int> channels;
  if (0 == ssrc) {  // Collect all channels, including the default one.
    // Default channel is not in receive_channels_ if it is not being used for
    // playout.
    if (default_receive_ssrc_ == 0)
      channels.push_back(voe_channel());
    for (ChannelMap::const_iterator it = receive_channels_.begin();
         it != receive_channels_.end(); ++it) {
      channels.push_back(it->second.channel);
    }
  } else {  // Collect only the channel of the specified ssrc.
    int channel = GetReceiveChannelNum(ssrc);
    if (-1 == channel) {
      LOG(LS_WARNING) << "Cannot find channel for ssrc:" << ssrc;
      return false;
    }
    channels.push_back(channel);
  }

  // Scale the output volume for the collected channels. We first normalize to
  // scale the volume and then set the left and right pan.
  float scale = static_cast<float>(talk_base::_max(left, right));
  if (scale > 0.0001f) {
    left /= scale;
    right /= scale;
  }
  for (std::vector<int>::const_iterator it = channels.begin();
      it != channels.end(); ++it) {
    if (-1 == engine()->voe()->volume()->SetChannelOutputVolumeScaling(
        *it, scale)) {
      LOG_RTCERR2(SetChannelOutputVolumeScaling, *it, scale);
      return false;
    }
    if (-1 == engine()->voe()->volume()->SetOutputVolumePan(
        *it, static_cast<float>(left), static_cast<float>(right))) {
      LOG_RTCERR3(SetOutputVolumePan, *it, left, right);
      // Do not return if fails. SetOutputVolumePan is not available for all
      // pltforms.
    }
    LOG(LS_INFO) << "SetOutputScaling to left=" << left * scale
                 << " right=" << right * scale
                 << " for channel " << *it << " and ssrc " << ssrc;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::GetOutputScaling(
    uint32 ssrc, double* left, double* right) {
  if (!left || !right) return false;

  talk_base::CritScope lock(&receive_channels_cs_);
  // Determine which channel based on ssrc.
  int channel = (0 == ssrc) ? voe_channel() : GetReceiveChannelNum(ssrc);
  if (channel == -1) {
    LOG(LS_WARNING) << "Cannot find channel for ssrc:" << ssrc;
    return false;
  }

  float scaling;
  if (-1 == engine()->voe()->volume()->GetChannelOutputVolumeScaling(
      channel, scaling)) {
    LOG_RTCERR2(GetChannelOutputVolumeScaling, channel, scaling);
    return false;
  }

  float left_pan;
  float right_pan;
  if (-1 == engine()->voe()->volume()->GetOutputVolumePan(
      channel, left_pan, right_pan)) {
    LOG_RTCERR3(GetOutputVolumePan, channel, left_pan, right_pan);
    // If GetOutputVolumePan fails, we use the default left and right pan.
    left_pan = 1.0f;
    right_pan = 1.0f;
  }

  *left = scaling * left_pan;
  *right = scaling * right_pan;
  return true;
}

bool WebRtcVoiceMediaChannel::SetRingbackTone(const char *buf, int len) {
  ringback_tone_.reset(new WebRtcSoundclipStream(buf, len));
  return true;
}

bool WebRtcVoiceMediaChannel::PlayRingbackTone(uint32 ssrc,
                                             bool play, bool loop) {
  if (!ringback_tone_) {
    return false;
  }

  // The voe file api is not available in chrome.
  if (!engine()->voe()->file()) {
    return false;
  }

  // Determine which VoiceEngine channel to play on.
  int channel = (ssrc == 0) ? voe_channel() : GetReceiveChannelNum(ssrc);
  if (channel == -1) {
    return false;
  }

  // Make sure the ringtone is cued properly, and play it out.
  if (play) {
    ringback_tone_->set_loop(loop);
    ringback_tone_->Rewind();
    if (engine()->voe()->file()->StartPlayingFileLocally(channel,
        ringback_tone_.get()) == -1) {
      LOG_RTCERR2(StartPlayingFileLocally, channel, ringback_tone_.get());
      LOG(LS_ERROR) << "Unable to start ringback tone";
      return false;
    }
    ringback_channels_.insert(channel);
    LOG(LS_INFO) << "Started ringback on channel " << channel;
  } else {
    if (engine()->voe()->file()->IsPlayingFileLocally(channel) == 1 &&
        engine()->voe()->file()->StopPlayingFileLocally(channel) == -1) {
      LOG_RTCERR1(StopPlayingFileLocally, channel);
      return false;
    }
    LOG(LS_INFO) << "Stopped ringback on channel " << channel;
    ringback_channels_.erase(channel);
  }

  return true;
}

bool WebRtcVoiceMediaChannel::CanInsertDtmf() {
  return dtmf_allowed_;
}

bool WebRtcVoiceMediaChannel::InsertDtmf(uint32 ssrc, int event,
                                         int duration, int flags) {
  if (!dtmf_allowed_) {
    return false;
  }

  // Send the event.
  if (flags & cricket::DF_SEND) {
    int channel = -1;
    if (ssrc == 0) {
      bool default_channel_is_inuse = false;
      for (ChannelMap::const_iterator iter = send_channels_.begin();
           iter != send_channels_.end(); ++iter) {
        if (IsDefaultChannel(iter->second.channel)) {
          default_channel_is_inuse = true;
          break;
        }
      }
      if (default_channel_is_inuse) {
        channel = voe_channel();
      } else if (!send_channels_.empty()) {
        channel = send_channels_.begin()->second.channel;
      }
    } else {
      channel = GetSendChannelNum(ssrc);
    }
    if (channel == -1) {
      LOG(LS_WARNING) << "InsertDtmf - The specified ssrc "
                      << ssrc << " is not in use.";
      return false;
    }
    // Send DTMF using out-of-band DTMF. ("true", as 3rd arg)
    if (engine()->voe()->dtmf()->SendTelephoneEvent(
            channel, event, true, duration) == -1) {
      LOG_RTCERR4(SendTelephoneEvent, channel, event, true, duration);
      return false;
    }
  }

  // Play the event.
  if (flags & cricket::DF_PLAY) {
    // Play DTMF tone locally.
    if (engine()->voe()->dtmf()->PlayDtmfTone(event, duration) == -1) {
      LOG_RTCERR2(PlayDtmfTone, event, duration);
      return false;
    }
  }

  return true;
}

void WebRtcVoiceMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  // Pick which channel to send this packet to. If this packet doesn't match
  // any multiplexed streams, just send it to the default channel. Otherwise,
  // send it to the specific decoder instance for that stream.
  int which_channel = GetReceiveChannelNum(
      ParseSsrc(packet->data(), packet->length(), false));
  if (which_channel == -1) {
    which_channel = voe_channel();
  }

  // Stop any ringback that might be playing on the channel.
  // It's possible the ringback has already stopped, ih which case we'll just
  // use the opportunity to remove the channel from ringback_channels_.
  if (engine()->voe()->file()) {
    const std::set<int>::iterator it = ringback_channels_.find(which_channel);
    if (it != ringback_channels_.end()) {
      if (engine()->voe()->file()->IsPlayingFileLocally(
          which_channel) == 1) {
        engine()->voe()->file()->StopPlayingFileLocally(which_channel);
        LOG(LS_INFO) << "Stopped ringback on channel " << which_channel
                     << " due to incoming media";
      }
      ringback_channels_.erase(which_channel);
    }
  }

  // Pass it off to the decoder.
  engine()->voe()->network()->ReceivedRTPPacket(
      which_channel,
      packet->data(),
      static_cast<unsigned int>(packet->length()));
}

void WebRtcVoiceMediaChannel::OnRtcpReceived(talk_base::Buffer* packet) {
  // Sending channels need all RTCP packets with feedback information.
  // Even sender reports can contain attached report blocks.
  // Receiving channels need sender reports in order to create
  // correct receiver reports.
  int type = 0;
  if (!GetRtcpType(packet->data(), packet->length(), &type)) {
    LOG(LS_WARNING) << "Failed to parse type from received RTCP packet";
    return;
  }

  // If it is a sender report, find the channel that is listening.
  bool has_sent_to_default_channel = false;
  if (type == kRtcpTypeSR) {
    int which_channel = GetReceiveChannelNum(
        ParseSsrc(packet->data(), packet->length(), true));
    if (which_channel != -1) {
      engine()->voe()->network()->ReceivedRTCPPacket(
          which_channel,
          packet->data(),
          static_cast<unsigned int>(packet->length()));

      if (IsDefaultChannel(which_channel))
        has_sent_to_default_channel = true;
    }
  }

  // SR may continue RR and any RR entry may correspond to any one of the send
  // channels. So all RTCP packets must be forwarded all send channels. VoE
  // will filter out RR internally.
  for (ChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    // Make sure not sending the same packet to default channel more than once.
    if (IsDefaultChannel(iter->second.channel) && has_sent_to_default_channel)
      continue;

    engine()->voe()->network()->ReceivedRTCPPacket(
        iter->second.channel,
        packet->data(),
        static_cast<unsigned int>(packet->length()));
  }
}

bool WebRtcVoiceMediaChannel::MuteStream(uint32 ssrc, bool muted) {
  int channel = (ssrc == 0) ? voe_channel() : GetSendChannelNum(ssrc);
  if (channel == -1) {
    LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  if (engine()->voe()->volume()->SetInputMute(channel, muted) == -1) {
    LOG_RTCERR2(SetInputMute, channel, muted);
    return false;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::SetSendBandwidth(bool autobw, int bps) {
  LOG(LS_INFO) << "WebRtcVoiceMediaChanne::SetSendBandwidth.";

  if (!send_codec_) {
    LOG(LS_INFO) << "The send codec has not been set up yet.";
    return false;
  }

  // Bandwidth is auto by default.
  if (autobw || bps <= 0)
    return true;

  webrtc::CodecInst codec = *send_codec_;
  bool is_multi_rate = IsCodecMultiRate(codec);

  if (is_multi_rate) {
    // If codec is multi-rate then just set the bitrate.
    codec.rate = bps;
    if (!SetSendCodec(codec)) {
      LOG(LS_INFO) << "Failed to set codec " << codec.plname
                   << " to bitrate " << bps << " bps.";
      return false;
    }
    return true;
  } else {
    // If codec is not multi-rate and |bps| is less than the fixed bitrate
    // then fail. If codec is not multi-rate and |bps| exceeds or equal the
    // fixed bitrate then ignore.
    if (bps < codec.rate) {
      LOG(LS_INFO) << "Failed to set codec " << codec.plname
                   << " to bitrate " << bps << " bps"
                   << ", requires at least " << codec.rate << " bps.";
      return false;
    }
    return true;
  }
}

bool WebRtcVoiceMediaChannel::GetStats(VoiceMediaInfo* info) {
  bool echo_metrics_on = false;
  // These can take on valid negative values, so use the lowest possible level
  // as default rather than -1.
  int echo_return_loss = -100;
  int echo_return_loss_enhancement = -100;
  // These can also be negative, but in practice -1 is only used to signal
  // insufficient data, since the resolution is limited to multiples of 4 ms.
  int echo_delay_median_ms = -1;
  int echo_delay_std_ms = -1;
  if (engine()->voe()->processing()->GetEcMetricsStatus(
          echo_metrics_on) != -1 && echo_metrics_on) {
    // TODO(ajm): we may want to use VoECallReport::GetEchoMetricsSummary
    // here, but it appears to be unsuitable currently. Revisit after this is
    // investigated: http://b/issue?id=5666755
    int erl, erle, rerl, anlp;
    if (engine()->voe()->processing()->GetEchoMetrics(
            erl, erle, rerl, anlp) != -1) {
      echo_return_loss = erl;
      echo_return_loss_enhancement = erle;
    }

    int median, std;
    if (engine()->voe()->processing()->GetEcDelayMetrics(median, std) != -1) {
      echo_delay_median_ms = median;
      echo_delay_std_ms = std;
    }
  }

  webrtc::CallStatistics cs;
  unsigned int ssrc;
  webrtc::CodecInst codec;
  unsigned int level;

  for (ChannelMap::const_iterator channel_iter = send_channels_.begin();
       channel_iter != send_channels_.end(); ++channel_iter) {
    const int channel = channel_iter->second.channel;

    // Fill in the sender info, based on what we know, and what the
    // remote side told us it got from its RTCP report.
    VoiceSenderInfo sinfo;

    if (engine()->voe()->rtp()->GetRTCPStatistics(channel, cs) == -1 ||
        engine()->voe()->rtp()->GetLocalSSRC(channel, ssrc) == -1) {
      continue;
    }

    sinfo.ssrc = ssrc;
    sinfo.codec_name = send_codec_.get() ? send_codec_->plname : "";
    sinfo.bytes_sent = cs.bytesSent;
    sinfo.packets_sent = cs.packetsSent;
    // RTT isn't known until a RTCP report is received. Until then, VoiceEngine
    // returns 0 to indicate an error value.
    sinfo.rtt_ms = (cs.rttMs > 0) ? cs.rttMs : -1;

    // Get data from the last remote RTCP report. Use default values if no data
    // available.
    sinfo.fraction_lost = -1.0;
    sinfo.jitter_ms = -1;
    sinfo.packets_lost = -1;
    sinfo.ext_seqnum = -1;
    std::vector<webrtc::ReportBlock> receive_blocks;
    if (engine()->voe()->rtp()->GetRemoteRTCPReportBlocks(
            channel, &receive_blocks) != -1 &&
        engine()->voe()->codec()->GetSendCodec(channel, codec) != -1) {
      std::vector<webrtc::ReportBlock>::iterator iter;
      for (iter = receive_blocks.begin(); iter != receive_blocks.end();
           ++iter) {
        // Lookup report for send ssrc only.
        if (iter->source_SSRC == sinfo.ssrc) {
          // Convert Q8 to floating point.
          sinfo.fraction_lost = static_cast<float>(iter->fraction_lost) / 256;
          // Convert samples to milliseconds.
          if (codec.plfreq / 1000 > 0) {
            sinfo.jitter_ms = iter->interarrival_jitter / (codec.plfreq / 1000);
          }
          sinfo.packets_lost = iter->cumulative_num_packets_lost;
          sinfo.ext_seqnum = iter->extended_highest_sequence_number;
          break;
        }
      }
    }

    // Local speech level.
    sinfo.audio_level = (engine()->voe()->volume()->
        GetSpeechInputLevelFullRange(level) != -1) ? level : -1;

    // TODO(xians): We are injecting the same APM logging to all the send
    // channels here because there is no good way to know which send channel
    // is using the APM. The correct fix is to allow the send channels to have
    // their own APM so that we can feed the correct APM logging to different
    // send channels. See issue crbug/264611 .
    sinfo.echo_return_loss = echo_return_loss;
    sinfo.echo_return_loss_enhancement = echo_return_loss_enhancement;
    sinfo.echo_delay_median_ms = echo_delay_median_ms;
    sinfo.echo_delay_std_ms = echo_delay_std_ms;
    // TODO(ajm): Re-enable this metric once we have a reliable implementation.
    sinfo.aec_quality_min = -1;
    sinfo.typing_noise_detected = typing_noise_detected_;

    info->senders.push_back(sinfo);
  }

  // Build the list of receivers, one for each receiving channel, or 1 in
  // a 1:1 call.
  std::vector<int> channels;
  for (ChannelMap::const_iterator it = receive_channels_.begin();
       it != receive_channels_.end(); ++it) {
    channels.push_back(it->second.channel);
  }
  if (channels.empty()) {
    channels.push_back(voe_channel());
  }

  // Get the SSRC and stats for each receiver, based on our own calculations.
  for (std::vector<int>::const_iterator it = channels.begin();
       it != channels.end(); ++it) {
    memset(&cs, 0, sizeof(cs));
    if (engine()->voe()->rtp()->GetRemoteSSRC(*it, ssrc) != -1 &&
        engine()->voe()->rtp()->GetRTCPStatistics(*it, cs) != -1 &&
        engine()->voe()->codec()->GetRecCodec(*it, codec) != -1) {
      VoiceReceiverInfo rinfo;
      rinfo.ssrc = ssrc;
      rinfo.bytes_rcvd = cs.bytesReceived;
      rinfo.packets_rcvd = cs.packetsReceived;
      // The next four fields are from the most recently sent RTCP report.
      // Convert Q8 to floating point.
      rinfo.fraction_lost = static_cast<float>(cs.fractionLost) / (1 << 8);
      rinfo.packets_lost = cs.cumulativeLost;
      rinfo.ext_seqnum = cs.extendedMax;
      // Convert samples to milliseconds.
      if (codec.plfreq / 1000 > 0) {
        rinfo.jitter_ms = cs.jitterSamples / (codec.plfreq / 1000);
      }

      // Get jitter buffer and total delay (alg + jitter + playout) stats.
      webrtc::NetworkStatistics ns;
      if (engine()->voe()->neteq() &&
          engine()->voe()->neteq()->GetNetworkStatistics(
              *it, ns) != -1) {
        rinfo.jitter_buffer_ms = ns.currentBufferSize;
        rinfo.jitter_buffer_preferred_ms = ns.preferredBufferSize;
        rinfo.expand_rate =
            static_cast<float>(ns.currentExpandRate) / (1 << 14);
      }
      if (engine()->voe()->sync()) {
        int playout_buffer_delay_ms = 0;
        engine()->voe()->sync()->GetDelayEstimate(
            *it, &rinfo.delay_estimate_ms, &playout_buffer_delay_ms);
      }

      // Get speech level.
      rinfo.audio_level = (engine()->voe()->volume()->
          GetSpeechOutputLevelFullRange(*it, level) != -1) ? level : -1;
      info->receivers.push_back(rinfo);
    }
  }

  return true;
}

void WebRtcVoiceMediaChannel::GetLastMediaError(
    uint32* ssrc, VoiceMediaChannel::Error* error) {
  ASSERT(ssrc != NULL);
  ASSERT(error != NULL);
  FindSsrc(voe_channel(), ssrc);
  *error = WebRtcErrorToChannelError(GetLastEngineError());
}

bool WebRtcVoiceMediaChannel::FindSsrc(int channel_num, uint32* ssrc) {
  talk_base::CritScope lock(&receive_channels_cs_);
  ASSERT(ssrc != NULL);
  if (channel_num == -1 && send_ != SEND_NOTHING) {
    // Sometimes the VoiceEngine core will throw error with channel_num = -1.
    // This means the error is not limited to a specific channel.  Signal the
    // message using ssrc=0.  If the current channel is sending, use this
    // channel for sending the message.
    *ssrc = 0;
    return true;
  } else {
    // Check whether this is a sending channel.
    for (ChannelMap::const_iterator it = send_channels_.begin();
         it != send_channels_.end(); ++it) {
      if (it->second.channel == channel_num) {
        // This is a sending channel.
        uint32 local_ssrc = 0;
        if (engine()->voe()->rtp()->GetLocalSSRC(
                channel_num, local_ssrc) != -1) {
          *ssrc = local_ssrc;
        }
        return true;
      }
    }

    // Check whether this is a receiving channel.
    for (ChannelMap::const_iterator it = receive_channels_.begin();
        it != receive_channels_.end(); ++it) {
      if (it->second.channel == channel_num) {
        *ssrc = it->first;
        return true;
      }
    }
  }
  return false;
}

void WebRtcVoiceMediaChannel::OnError(uint32 ssrc, int error) {
  if (error == VE_TYPING_NOISE_WARNING) {
    typing_noise_detected_ = true;
  } else if (error == VE_TYPING_NOISE_OFF_WARNING) {
    typing_noise_detected_ = false;
  }
  SignalMediaError(ssrc, WebRtcErrorToChannelError(error));
}

int WebRtcVoiceMediaChannel::GetOutputLevel(int channel) {
  unsigned int ulevel;
  int ret =
      engine()->voe()->volume()->GetSpeechOutputLevel(channel, ulevel);
  return (ret == 0) ? static_cast<int>(ulevel) : -1;
}

int WebRtcVoiceMediaChannel::GetReceiveChannelNum(uint32 ssrc) {
  ChannelMap::iterator it = receive_channels_.find(ssrc);
  if (it != receive_channels_.end())
    return it->second.channel;
  return (ssrc == default_receive_ssrc_) ?  voe_channel() : -1;
}

int WebRtcVoiceMediaChannel::GetSendChannelNum(uint32 ssrc) {
  ChannelMap::iterator it = send_channels_.find(ssrc);
  if (it != send_channels_.end())
    return it->second.channel;

  return -1;
}

bool WebRtcVoiceMediaChannel::GetRedSendCodec(const AudioCodec& red_codec,
    const std::vector<AudioCodec>& all_codecs, webrtc::CodecInst* send_codec) {
  // Get the RED encodings from the parameter with no name. This may
  // change based on what is discussed on the Jingle list.
  // The encoding parameter is of the form "a/b"; we only support where
  // a == b. Verify this and parse out the value into red_pt.
  // If the parameter value is absent (as it will be until we wire up the
  // signaling of this message), use the second codec specified (i.e. the
  // one after "red") as the encoding parameter.
  int red_pt = -1;
  std::string red_params;
  CodecParameterMap::const_iterator it = red_codec.params.find("");
  if (it != red_codec.params.end()) {
    red_params = it->second;
    std::vector<std::string> red_pts;
    if (talk_base::split(red_params, '/', &red_pts) != 2 ||
        red_pts[0] != red_pts[1] ||
        !talk_base::FromString(red_pts[0], &red_pt)) {
      LOG(LS_WARNING) << "RED params " << red_params << " not supported.";
      return false;
    }
  } else if (red_codec.params.empty()) {
    LOG(LS_WARNING) << "RED params not present, using defaults";
    if (all_codecs.size() > 1) {
      red_pt = all_codecs[1].id;
    }
  }

  // Try to find red_pt in |codecs|.
  std::vector<AudioCodec>::const_iterator codec;
  for (codec = all_codecs.begin(); codec != all_codecs.end(); ++codec) {
    if (codec->id == red_pt)
      break;
  }

  // If we find the right codec, that will be the codec we pass to
  // SetSendCodec, with the desired payload type.
  if (codec != all_codecs.end() &&
    engine()->FindWebRtcCodec(*codec, send_codec)) {
  } else {
    LOG(LS_WARNING) << "RED params " << red_params << " are invalid.";
    return false;
  }

  return true;
}

bool WebRtcVoiceMediaChannel::EnableRtcp(int channel) {
  if (engine()->voe()->rtp()->SetRTCPStatus(channel, true) == -1) {
    LOG_RTCERR2(SetRTCPStatus, channel, 1);
    return false;
  }
  // TODO(juberti): Enable VQMon and RTCP XR reports, once we know what
  // what we want to do with them.
  // engine()->voe().EnableVQMon(voe_channel(), true);
  // engine()->voe().EnableRTCP_XR(voe_channel(), true);
  return true;
}

bool WebRtcVoiceMediaChannel::ResetRecvCodecs(int channel) {
  int ncodecs = engine()->voe()->codec()->NumOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    webrtc::CodecInst voe_codec;
    if (engine()->voe()->codec()->GetCodec(i, voe_codec) != -1) {
      voe_codec.pltype = -1;
      if (engine()->voe()->codec()->SetRecPayloadType(
          channel, voe_codec) == -1) {
        LOG_RTCERR2(SetRecPayloadType, channel, ToString(voe_codec));
        return false;
      }
    }
  }
  return true;
}

bool WebRtcVoiceMediaChannel::SetPlayout(int channel, bool playout) {
  if (playout) {
    LOG(LS_INFO) << "Starting playout for channel #" << channel;
    if (engine()->voe()->base()->StartPlayout(channel) == -1) {
      LOG_RTCERR1(StartPlayout, channel);
      return false;
    }
  } else {
    LOG(LS_INFO) << "Stopping playout for channel #" << channel;
    engine()->voe()->base()->StopPlayout(channel);
  }
  return true;
}

uint32 WebRtcVoiceMediaChannel::ParseSsrc(const void* data, size_t len,
                                        bool rtcp) {
  size_t ssrc_pos = (!rtcp) ? 8 : 4;
  uint32 ssrc = 0;
  if (len >= (ssrc_pos + sizeof(ssrc))) {
    ssrc = talk_base::GetBE32(static_cast<const char*>(data) + ssrc_pos);
  }
  return ssrc;
}

// Convert VoiceEngine error code into VoiceMediaChannel::Error enum.
VoiceMediaChannel::Error
    WebRtcVoiceMediaChannel::WebRtcErrorToChannelError(int err_code) {
  switch (err_code) {
    case 0:
      return ERROR_NONE;
    case VE_CANNOT_START_RECORDING:
    case VE_MIC_VOL_ERROR:
    case VE_GET_MIC_VOL_ERROR:
    case VE_CANNOT_ACCESS_MIC_VOL:
      return ERROR_REC_DEVICE_OPEN_FAILED;
    case VE_SATURATION_WARNING:
      return ERROR_REC_DEVICE_SATURATION;
    case VE_REC_DEVICE_REMOVED:
      return ERROR_REC_DEVICE_REMOVED;
    case VE_RUNTIME_REC_WARNING:
    case VE_RUNTIME_REC_ERROR:
      return ERROR_REC_RUNTIME_ERROR;
    case VE_CANNOT_START_PLAYOUT:
    case VE_SPEAKER_VOL_ERROR:
    case VE_GET_SPEAKER_VOL_ERROR:
    case VE_CANNOT_ACCESS_SPEAKER_VOL:
      return ERROR_PLAY_DEVICE_OPEN_FAILED;
    case VE_RUNTIME_PLAY_WARNING:
    case VE_RUNTIME_PLAY_ERROR:
      return ERROR_PLAY_RUNTIME_ERROR;
    case VE_TYPING_NOISE_WARNING:
      return ERROR_REC_TYPING_NOISE_DETECTED;
    default:
      return VoiceMediaChannel::ERROR_OTHER;
  }
}

int WebRtcSoundclipStream::Read(void *buf, int len) {
  size_t res = 0;
  mem_.Read(buf, len, &res, NULL);
  return static_cast<int>(res);
}

int WebRtcSoundclipStream::Rewind() {
  mem_.Rewind();
  // Return -1 to keep VoiceEngine from looping.
  return (loop_) ? 0 : -1;
}

}  // namespace cricket

#endif  // HAVE_WEBRTC_VOICE
