/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "webrtc/modules/audio_device/ios/audio_device_ios.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_device/fine_audio_buffer.h"
#include "webrtc/modules/utility/interface/helpers_ios.h"

namespace webrtc {

#define LOGI() LOG(LS_INFO) << "AudioDeviceIOS::"

#define LOG_AND_RETURN_IF_ERROR(error, message) \
  do {                                          \
    OSStatus err = error;                       \
    if (err) {                                  \
      LOG(LS_ERROR) << message << ": " << err;  \
      return false;                             \
    }                                           \
  } while (0)

// Preferred hardware sample rate (unit is in Hertz). The client sample rate
// will be set to this value as well to avoid resampling the the audio unit's
// format converter. Note that, some devices, e.g. BT headsets, only supports
// 8000Hz as native sample rate.
const double kPreferredSampleRate = 48000.0;
// Use a hardware I/O buffer size (unit is in seconds) that matches the 10ms
// size used by WebRTC. The exact actual size will differ between devices.
// Example: using 48kHz on iPhone 6 results in a native buffer size of
// ~10.6667ms or 512 audio frames per buffer. The FineAudioBuffer instance will
// take care of any buffering required to convert between native buffers and
// buffers used by WebRTC. It is beneficial for the performance if the native
// size is as close to 10ms as possible since it results in "clean" callback
// sequence without bursts of callbacks back to back.
const double kPreferredIOBufferDuration = 0.01;
// Try to use mono to save resources. Also avoids channel format conversion
// in the I/O audio unit. Initial tests have shown that it is possible to use
// mono natively for built-in microphones and for BT headsets but not for
// wired headsets. Wired headsets only support stereo as native channel format
// but it is a low cost operation to do a format conversion to mono in the
// audio unit. Hence, we will not hit a RTC_CHECK in
// VerifyAudioParametersForActiveAudioSession() for a mismatch between the
// preferred number of channels and the actual number of channels.
const int kPreferredNumberOfChannels = 1;
// Number of bytes per audio sample for 16-bit signed integer representation.
const UInt32 kBytesPerSample = 2;
// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

using ios::CheckAndLogError;

// Activates an audio session suitable for full duplex VoIP sessions when
// |activate| is true. Also sets the preferred sample rate and IO buffer
// duration. Deactivates an active audio session if |activate| is set to false.
static void ActivateAudioSession(AVAudioSession* session, bool activate) {
  LOG(LS_INFO) << "ActivateAudioSession(" << activate << ")";
  @autoreleasepool {
    NSError* error = nil;
    BOOL success = NO;
    // Deactivate the audio session and return if |activate| is false.
    if (!activate) {
      success = [session setActive:NO error:&error];
      RTC_DCHECK(CheckAndLogError(success, error));
      return;
    }
    // Use a category which supports simultaneous recording and playback.
    // By default, using this category implies that our app’s audio is
    // nonmixable, hence activating the session will interrupt any other
    // audio sessions which are also nonmixable.
    if (session.category != AVAudioSessionCategoryPlayAndRecord) {
      error = nil;
      success = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                               error:&error];
      RTC_DCHECK(CheckAndLogError(success, error));
    }
    // Specify mode for two-way voice communication (e.g. VoIP).
    if (session.mode != AVAudioSessionModeVoiceChat) {
      error = nil;
      success = [session setMode:AVAudioSessionModeVoiceChat error:&error];
      RTC_DCHECK(CheckAndLogError(success, error));
    }
    // Set the session's sample rate or the hardware sample rate.
    // It is essential that we use the same sample rate as stream format
    // to ensure that the I/O unit does not have to do sample rate conversion.
    error = nil;
    success =
        [session setPreferredSampleRate:kPreferredSampleRate error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
    // Set the preferred audio I/O buffer duration, in seconds.
    // TODO(henrika): add more comments here.
    error = nil;
    success = [session setPreferredIOBufferDuration:kPreferredIOBufferDuration
                                              error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));

    // TODO(henrika): add observers here...

    // Activate the audio session. Activation can fail if another active audio
    // session (e.g. phone call) has higher priority than ours.
    error = nil;
    success = [session setActive:YES error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
    RTC_CHECK(session.isInputAvailable) << "No input path is available!";
    // Ensure that category and mode are actually activated.
    RTC_DCHECK(
        [session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]);
    RTC_DCHECK([session.mode isEqualToString:AVAudioSessionModeVoiceChat]);
    // Try to set the preferred number of hardware audio channels. These calls
    // must be done after setting the audio session’s category and mode and
    // activating the session.
    // We try to use mono in both directions to save resources and format
    // conversions in the audio unit. Some devices does only support stereo;
    // e.g. wired headset on iPhone 6.
    // TODO(henrika): add support for stereo if needed.
    error = nil;
    success =
        [session setPreferredInputNumberOfChannels:kPreferredNumberOfChannels
                                             error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
    error = nil;
    success =
        [session setPreferredOutputNumberOfChannels:kPreferredNumberOfChannels
                                              error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
  }
}

#if !defined(NDEBUG)
// Helper method for printing out an AudioStreamBasicDescription structure.
static void LogABSD(AudioStreamBasicDescription absd) {
  char formatIDString[5];
  UInt32 formatID = CFSwapInt32HostToBig(absd.mFormatID);
  bcopy(&formatID, formatIDString, 4);
  formatIDString[4] = '\0';
  LOG(LS_INFO) << "LogABSD";
  LOG(LS_INFO) << " sample rate: " << absd.mSampleRate;
  LOG(LS_INFO) << " format ID: " << formatIDString;
  LOG(LS_INFO) << " format flags: " << std::hex << absd.mFormatFlags;
  LOG(LS_INFO) << " bytes per packet: " << absd.mBytesPerPacket;
  LOG(LS_INFO) << " frames per packet: " << absd.mFramesPerPacket;
  LOG(LS_INFO) << " bytes per frame: " << absd.mBytesPerFrame;
  LOG(LS_INFO) << " channels per packet: " << absd.mChannelsPerFrame;
  LOG(LS_INFO) << " bits per channel: " << absd.mBitsPerChannel;
  LOG(LS_INFO) << " reserved: " << absd.mReserved;
}

// Helper method that logs essential device information strings.
static void LogDeviceInfo() {
  LOG(LS_INFO) << "LogDeviceInfo";
  @autoreleasepool {
    LOG(LS_INFO) << " system name: " << ios::GetSystemName();
    LOG(LS_INFO) << " system version: " << ios::GetSystemVersion();
    LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
  }
}
#endif  // !defined(NDEBUG)

AudioDeviceIOS::AudioDeviceIOS()
    : _audioDeviceBuffer(nullptr),
      _vpioUnit(nullptr),
      _recording(0),
      _playing(0),
      _initialized(false),
      _recIsInitialized(false),
      _playIsInitialized(false),
      _audioInterruptionObserver(nullptr) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription();
}

AudioDeviceIOS::~AudioDeviceIOS() {
  LOGI() << "~dtor";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  Terminate();
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  _audioDeviceBuffer = audioBuffer;
}

int32_t AudioDeviceIOS::Init() {
  LOGI() << "Init";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  if (_initialized) {
    return 0;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif
  // Store the preferred sample rate and preferred number of channels already
  // here. They have not been set and confirmed yet since ActivateAudioSession()
  // is not called until audio is about to start. However, it makes sense to
  // store the parameters now and then verify at a later stage.
  _playoutParameters.reset(kPreferredSampleRate, kPreferredNumberOfChannels);
  _recordParameters.reset(kPreferredSampleRate, kPreferredNumberOfChannels);
  // Ensure that the audio device buffer (ADB) knows about the internal audio
  // parameters. Note that, even if we are unable to get a mono audio session,
  // we will always tell the I/O audio unit to do a channel format conversion
  // to guarantee mono on the "input side" of the audio unit.
  UpdateAudioDeviceBuffer();
  _initialized = true;
  return 0;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  if (!_initialized) {
    return 0;
  }
  ShutdownPlayOrRecord();
  _initialized = false;
  return 0;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  RTC_DCHECK(_initialized);
  RTC_DCHECK(!_playIsInitialized);
  RTC_DCHECK(!_playing);
  if (!_recIsInitialized) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed!";
      return -1;
    }
  }
  _playIsInitialized = true;
  return 0;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  RTC_DCHECK(_initialized);
  RTC_DCHECK(!_recIsInitialized);
  RTC_DCHECK(!_recording);
  if (!_playIsInitialized) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed!";
      return -1;
    }
  }
  _recIsInitialized = true;
  return 0;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  RTC_DCHECK(_playIsInitialized);
  RTC_DCHECK(!_playing);
  _fineAudioBuffer->ResetPlayout();
  if (!_recording) {
    OSStatus result = AudioOutputUnitStart(_vpioUnit);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed: " << result;
      return -1;
    }
  }
  rtc::AtomicOps::ReleaseStore(&_playing, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  if (!_playIsInitialized || !_playing) {
    return 0;
  }
  if (!_recording) {
    ShutdownPlayOrRecord();
  }
  _playIsInitialized = false;
  rtc::AtomicOps::ReleaseStore(&_playing, 0);
  return 0;
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  RTC_DCHECK(_recIsInitialized);
  RTC_DCHECK(!_recording);
  _fineAudioBuffer->ResetRecord();
  if (!_playing) {
    OSStatus result = AudioOutputUnitStart(_vpioUnit);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed: " << result;
      return -1;
    }
  }
  rtc::AtomicOps::ReleaseStore(&_recording, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  if (!_recIsInitialized || !_recording) {
    return 0;
  }
  if (!_playing) {
    ShutdownPlayOrRecord();
  }
  _recIsInitialized = false;
  rtc::AtomicOps::ReleaseStore(&_recording, 0);
  return 0;
}

// Change the default receiver playout route to speaker.
int32_t AudioDeviceIOS::SetLoudspeakerStatus(bool enable) {
  LOGI() << "SetLoudspeakerStatus(" << enable << ")";

  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSString* category = session.category;
  AVAudioSessionCategoryOptions options = session.categoryOptions;
  // Respect old category options if category is
  // AVAudioSessionCategoryPlayAndRecord. Otherwise reset it since old options
  // might not be valid for this category.
  if ([category isEqualToString:AVAudioSessionCategoryPlayAndRecord]) {
    if (enable) {
      options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
    } else {
      options &= ~AVAudioSessionCategoryOptionDefaultToSpeaker;
    }
  } else {
    options = AVAudioSessionCategoryOptionDefaultToSpeaker;
  }
  NSError* error = nil;
  BOOL success = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                          withOptions:options
                                error:&error];
  ios::CheckAndLogError(success, error);
  return (error == nil) ? 0 : -1;
}

int32_t AudioDeviceIOS::GetLoudspeakerStatus(bool& enabled) const {
  LOGI() << "GetLoudspeakerStatus";
  AVAudioSession* session = [AVAudioSession sharedInstance];
  AVAudioSessionCategoryOptions options = session.categoryOptions;
  enabled = options & AVAudioSessionCategoryOptionDefaultToSpeaker;
  return 0;
}

int32_t AudioDeviceIOS::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int32_t AudioDeviceIOS::RecordingDelay(uint16_t& delayMS) const {
  delayMS = kFixedRecordDelayEstimate;
  return 0;
}

int AudioDeviceIOS::GetPlayoutAudioParameters(AudioParameters* params) const {
  LOGI() << "GetPlayoutAudioParameters";
  RTC_DCHECK(_playoutParameters.is_valid());
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  *params = _playoutParameters;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  LOGI() << "GetRecordAudioParameters";
  RTC_DCHECK(_recordParameters.is_valid());
  RTC_DCHECK(_threadChecker.CalledOnValidThread());
  *params = _recordParameters;
  return 0;
}

void AudioDeviceIOS::UpdateAudioDeviceBuffer() {
  LOGI() << "UpdateAudioDevicebuffer";
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(_audioDeviceBuffer) << "AttachAudioBuffer must be called first";
  // Inform the audio device buffer (ADB) about the new audio format.
  _audioDeviceBuffer->SetPlayoutSampleRate(_playoutParameters.sample_rate());
  _audioDeviceBuffer->SetPlayoutChannels(_playoutParameters.channels());
  _audioDeviceBuffer->SetRecordingSampleRate(_recordParameters.sample_rate());
  _audioDeviceBuffer->SetRecordingChannels(_recordParameters.channels());
}

void AudioDeviceIOS::SetupAudioBuffersForActiveAudioSession() {
  LOGI() << "SetupAudioBuffersForActiveAudioSession";
  AVAudioSession* session = [AVAudioSession sharedInstance];
  // Verify the current values once the audio session has been activated.
  LOG(LS_INFO) << " sample rate: " << session.sampleRate;
  LOG(LS_INFO) << " IO buffer duration: " << session.IOBufferDuration;
  LOG(LS_INFO) << " output channels: " << session.outputNumberOfChannels;
  LOG(LS_INFO) << " input channels: " << session.inputNumberOfChannels;
  LOG(LS_INFO) << " output latency: " << session.outputLatency;
  LOG(LS_INFO) << " input latency: " << session.inputLatency;
  // Log a warning message for the case when we are unable to set the preferred
  // hardware sample rate but continue and use the non-ideal sample rate after
  // reinitializing the audio parameters.
  if (session.sampleRate != _playoutParameters.sample_rate()) {
    LOG(LS_WARNING)
        << "Failed to enable an audio session with the preferred sample rate!";
  }

  // At this stage, we also know the exact IO buffer duration and can add
  // that info to the existing audio parameters where it is converted into
  // number of audio frames.
  // Example: IO buffer size = 0.008 seconds <=> 128 audio frames at 16kHz.
  // Hence, 128 is the size we expect to see in upcoming render callbacks.
  _playoutParameters.reset(session.sampleRate, _playoutParameters.channels(),
                           session.IOBufferDuration);
  RTC_DCHECK(_playoutParameters.is_complete());
  _recordParameters.reset(session.sampleRate, _recordParameters.channels(),
                          session.IOBufferDuration);
  RTC_DCHECK(_recordParameters.is_complete());
  LOG(LS_INFO) << " frames per I/O buffer: "
               << _playoutParameters.frames_per_buffer();
  LOG(LS_INFO) << " bytes per I/O buffer: "
               << _playoutParameters.GetBytesPerBuffer();
  RTC_DCHECK_EQ(_playoutParameters.GetBytesPerBuffer(),
                _recordParameters.GetBytesPerBuffer());

  // Update the ADB parameters since the sample rate might have changed.
  UpdateAudioDeviceBuffer();

  // Create a modified audio buffer class which allows us to ask for,
  // or deliver, any number of samples (and not only multiple of 10ms) to match
  // the native audio unit buffer size.
  RTC_DCHECK(_audioDeviceBuffer);
  _fineAudioBuffer.reset(new FineAudioBuffer(
      _audioDeviceBuffer, _playoutParameters.GetBytesPerBuffer(),
      _playoutParameters.sample_rate()));

  // The extra/temporary playoutbuffer must be of this size to avoid
  // unnecessary memcpy while caching data between successive callbacks.
  const int requiredPlayoutBufferSize =
      _fineAudioBuffer->RequiredPlayoutBufferSizeBytes();
  LOG(LS_INFO) << " required playout buffer size: "
               << requiredPlayoutBufferSize;
  _playoutAudioBuffer.reset(new SInt8[requiredPlayoutBufferSize]);

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer
  // in |_recordAudioBuffer|. Recorded audio will be rendered into this memory
  // at each input callback when calling AudioUnitRender().
  const int dataByteSize = _recordParameters.GetBytesPerBuffer();
  _recordAudioBuffer.reset(new SInt8[dataByteSize]);
  _audioRecordBufferList.mNumberBuffers = 1;
  AudioBuffer* audioBuffer = &_audioRecordBufferList.mBuffers[0];
  audioBuffer->mNumberChannels = _recordParameters.channels();
  audioBuffer->mDataByteSize = dataByteSize;
  audioBuffer->mData = _recordAudioBuffer.get();
}

bool AudioDeviceIOS::SetupAndInitializeVoiceProcessingAudioUnit() {
  LOGI() << "SetupAndInitializeVoiceProcessingAudioUnit";
  RTC_DCHECK(!_vpioUnit);
  // Create an audio component description to identify the Voice-Processing
  // I/O audio unit.
  AudioComponentDescription vpioUnitDescription;
  vpioUnitDescription.componentType = kAudioUnitType_Output;
  vpioUnitDescription.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
  vpioUnitDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
  vpioUnitDescription.componentFlags = 0;
  vpioUnitDescription.componentFlagsMask = 0;
  // Obtain an audio unit instance given the description.
  AudioComponent foundVpioUnitRef =
      AudioComponentFindNext(nullptr, &vpioUnitDescription);

  // Create a Voice-Processing IO audio unit.
  LOG_AND_RETURN_IF_ERROR(
      AudioComponentInstanceNew(foundVpioUnitRef, &_vpioUnit),
      "Failed to create a VoiceProcessingIO audio unit");

  // A VP I/O unit's bus 1 connects to input hardware (microphone). Enable
  // input on the input scope of the input element.
  AudioUnitElement inputBus = 1;
  UInt32 enableInput = 1;
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Input, inputBus, &enableInput,
                           sizeof(enableInput)),
      "Failed to enable input on input scope of input element");

  // A VP I/O unit's bus 0 connects to output hardware (speaker). Enable
  // output on the output scope of the output element.
  AudioUnitElement outputBus = 0;
  UInt32 enableOutput = 1;
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Output, outputBus, &enableOutput,
                           sizeof(enableOutput)),
      "Failed to enable output on output scope of output element");

  // Set the application formats for input and output:
  // - use same format in both directions
  // - avoid resampling in the I/O unit by using the hardware sample rate
  // - linear PCM => noncompressed audio data format with one frame per packet
  // - no need to specify interleaving since only mono is supported
  AudioStreamBasicDescription applicationFormat = {0};
  UInt32 size = sizeof(applicationFormat);
  RTC_DCHECK_EQ(_playoutParameters.sample_rate(),
                _recordParameters.sample_rate());
  RTC_DCHECK_EQ(1, kPreferredNumberOfChannels);
  applicationFormat.mSampleRate = _playoutParameters.sample_rate();
  applicationFormat.mFormatID = kAudioFormatLinearPCM;
  applicationFormat.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  applicationFormat.mBytesPerPacket = kBytesPerSample;
  applicationFormat.mFramesPerPacket = 1;  // uncompressed
  applicationFormat.mBytesPerFrame = kBytesPerSample;
  applicationFormat.mChannelsPerFrame = kPreferredNumberOfChannels;
  applicationFormat.mBitsPerChannel = 8 * kBytesPerSample;
#if !defined(NDEBUG)
  LogABSD(applicationFormat);
#endif

  // Set the application format on the output scope of the input element/bus.
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Output, inputBus, &applicationFormat,
                           size),
      "Failed to set application format on output scope of input element");

  // Set the application format on the input scope of the output element/bus.
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, outputBus, &applicationFormat,
                           size),
      "Failed to set application format on input scope of output element");

  // Specify the callback function that provides audio samples to the audio
  // unit.
  AURenderCallbackStruct renderCallback;
  renderCallback.inputProc = GetPlayoutData;
  renderCallback.inputProcRefCon = this;
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Input, outputBus, &renderCallback,
                           sizeof(renderCallback)),
      "Failed to specify the render callback on the output element");

  // Disable AU buffer allocation for the recorder, we allocate our own.
  // TODO(henrika): not sure that it actually saves resource to make this call.
  UInt32 flag = 0;
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioUnitProperty_ShouldAllocateBuffer,
                           kAudioUnitScope_Output, inputBus, &flag,
                           sizeof(flag)),
      "Failed to disable buffer allocation on the input element");

  // Specify the callback to be called by the I/O thread to us when input audio
  // is available. The recorded samples can then be obtained by calling the
  // AudioUnitRender() method.
  AURenderCallbackStruct inputCallback;
  inputCallback.inputProc = RecordedDataIsAvailable;
  inputCallback.inputProcRefCon = this;
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitSetProperty(_vpioUnit, kAudioOutputUnitProperty_SetInputCallback,
                           kAudioUnitScope_Global, inputBus, &inputCallback,
                           sizeof(inputCallback)),
      "Failed to specify the input callback on the input element");

  // Initialize the Voice-Processing I/O unit instance.
  LOG_AND_RETURN_IF_ERROR(AudioUnitInitialize(_vpioUnit),
                          "Failed to initialize the Voice-Processing I/O unit");
  return true;
}

bool AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";
  AVAudioSession* session = [AVAudioSession sharedInstance];
  // Activate the audio session and ask for a set of preferred audio parameters.
  ActivateAudioSession(session, true);

  // Ensure that we got what what we asked for in our active audio session.
  SetupAudioBuffersForActiveAudioSession();

  // Create, setup and initialize a new Voice-Processing I/O unit.
  if (!SetupAndInitializeVoiceProcessingAudioUnit()) {
    return false;
  }

  // Listen to audio interruptions.
  // TODO(henrika): learn this area better.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  id observer = [center
      addObserverForName:AVAudioSessionInterruptionNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* notification) {
                NSNumber* typeNumber =
                    [notification userInfo][AVAudioSessionInterruptionTypeKey];
                AVAudioSessionInterruptionType type =
                    (AVAudioSessionInterruptionType)[typeNumber
                                                         unsignedIntegerValue];
                switch (type) {
                  case AVAudioSessionInterruptionTypeBegan:
                    // At this point our audio session has been deactivated and
                    // the
                    // audio unit render callbacks no longer occur. Nothing to
                    // do.
                    break;
                  case AVAudioSessionInterruptionTypeEnded: {
                    NSError* error = nil;
                    AVAudioSession* session = [AVAudioSession sharedInstance];
                    [session setActive:YES error:&error];
                    if (error != nil) {
                      LOG_F(LS_ERROR) << "Failed to active audio session";
                    }
                    // Post interruption the audio unit render callbacks don't
                    // automatically continue, so we restart the unit manually
                    // here.
                    AudioOutputUnitStop(_vpioUnit);
                    AudioOutputUnitStart(_vpioUnit);
                    break;
                  }
                }
              }];
  // Increment refcount on observer using ARC bridge. Instance variable is a
  // void* instead of an id because header is included in other pure C++
  // files.
  _audioInterruptionObserver = (__bridge_retained void*)observer;
  return true;
}

bool AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
  if (_audioInterruptionObserver != nullptr) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    // Transfer ownership of observer back to ARC, which will dealloc the
    // observer once it exits this scope.
    id observer = (__bridge_transfer id)_audioInterruptionObserver;
    [center removeObserver:observer];
    _audioInterruptionObserver = nullptr;
  }
  // Close and delete the voice-processing I/O unit.
  OSStatus result = -1;
  if (nullptr != _vpioUnit) {
    result = AudioOutputUnitStop(_vpioUnit);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStop failed: " << result;
    }
    result = AudioComponentInstanceDispose(_vpioUnit);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioComponentInstanceDispose failed: " << result;
    }
    _vpioUnit = nullptr;
  }
  // All I/O should be stopped or paused prior to deactivating the audio
  // session, hence we deactivate as last action.
  AVAudioSession* session = [AVAudioSession sharedInstance];
  ActivateAudioSession(session, false);
  return true;
}

OSStatus AudioDeviceIOS::RecordedDataIsAvailable(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {
  RTC_DCHECK_EQ(1u, inBusNumber);
  RTC_DCHECK(!ioData);  // no buffer should be allocated for input at this stage
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(inRefCon);
  return audio_device_ios->OnRecordedDataIsAvailable(
      ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames);
}

OSStatus AudioDeviceIOS::OnRecordedDataIsAvailable(
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames) {
  RTC_DCHECK_EQ(_recordParameters.frames_per_buffer(), inNumberFrames);
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&_recording))
    return result;
  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the |ioData| parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // TODO(henrika): should error handling be improved?
  AudioBufferList* ioData = &_audioRecordBufferList;
  result = AudioUnitRender(_vpioUnit, ioActionFlags, inTimeStamp, inBusNumber,
                           inNumberFrames, ioData);
  if (result != noErr) {
    LOG_F(LS_ERROR) << "AudioOutputUnitStart failed: " << result;
    return result;
  }
  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  const UInt32 dataSizeInBytes = ioData->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(dataSizeInBytes / kBytesPerSample, inNumberFrames);
  SInt8* data = static_cast<SInt8*>(ioData->mBuffers[0].mData);
  _fineAudioBuffer->DeliverRecordedData(data, dataSizeInBytes,
                                        kFixedPlayoutDelayEstimate,
                                        kFixedRecordDelayEstimate);
  return noErr;
}

OSStatus AudioDeviceIOS::GetPlayoutData(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {
  RTC_DCHECK_EQ(0u, inBusNumber);
  RTC_DCHECK(ioData);
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(inRefCon);
  return audio_device_ios->OnGetPlayoutData(ioActionFlags, inNumberFrames,
                                            ioData);
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(
    AudioUnitRenderActionFlags* ioActionFlags,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {
  // Verify 16-bit, noninterleaved mono PCM signal format.
  RTC_DCHECK_EQ(1u, ioData->mNumberBuffers);
  RTC_DCHECK_EQ(1u, ioData->mBuffers[0].mNumberChannels);
  // Get pointer to internal audio buffer to which new audio data shall be
  // written.
  const UInt32 dataSizeInBytes = ioData->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(dataSizeInBytes / kBytesPerSample, inNumberFrames);
  SInt8* destination = static_cast<SInt8*>(ioData->mBuffers[0].mData);
  // Produce silence and give audio unit a hint about it if playout is not
  // activated.
  if (!rtc::AtomicOps::AcquireLoad(&_playing)) {
    *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(destination, 0, dataSizeInBytes);
    return noErr;
  }
  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) to a preallocated intermediate buffer and
  // copy the result to the audio buffer in the |ioData| destination.
  SInt8* source = _playoutAudioBuffer.get();
  _fineAudioBuffer->GetPlayoutData(source);
  memcpy(destination, source, dataSizeInBytes);
  return noErr;
}

}  // namespace webrtc
