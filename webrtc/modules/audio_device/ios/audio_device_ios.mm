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
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_device/fine_audio_buffer.h"
#include "webrtc/modules/utility/include/helpers_ios.h"

#import "webrtc/base/objc/RTCLogging.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession+Private.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSessionConfiguration.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSessionDelegateAdapter.h"

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

#define LOG_IF_ERROR(error, message)           \
  do {                                         \
    OSStatus err = error;                      \
    if (err) {                                 \
      LOG(LS_ERROR) << message << ": " << err; \
    }                                          \
  } while (0)


// Number of bytes per audio sample for 16-bit signed integer representation.
const UInt32 kBytesPerSample = 2;
// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;
// Calls to AudioUnitInitialize() can fail if called back-to-back on different
// ADM instances. A fall-back solution is to allow multiple sequential calls
// with as small delay between each. This factor sets the max number of allowed
// initialization attempts.
const int kMaxNumberOfAudioUnitInitializeAttempts = 5;

using ios::CheckAndLogError;

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
    LOG(LS_INFO) << " system version 1(2): " << ios::GetSystemVersionAsString();
    LOG(LS_INFO) << " system version 2(2): " << ios::GetSystemVersion();
    LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
    LOG(LS_INFO) << " process name: " << ios::GetProcessName();
    LOG(LS_INFO) << " process ID: " << ios::GetProcessID();
    LOG(LS_INFO) << " OS version: " << ios::GetOSVersionString();
    LOG(LS_INFO) << " processing cores: " << ios::GetProcessorCount();
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
    LOG(LS_INFO) << " low power mode: " << ios::GetLowPowerModeEnabled();
#endif
  }
}
#endif  // !defined(NDEBUG)

AudioDeviceIOS::AudioDeviceIOS()
  : async_invoker_(new rtc::AsyncInvoker()),
    audio_device_buffer_(nullptr),
    vpio_unit_(nullptr),
    recording_(0),
    playing_(0),
    initialized_(false),
    rec_is_initialized_(false),
    play_is_initialized_(false),
    is_interrupted_(false) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription();
  thread_ = rtc::Thread::Current();
  audio_session_observer_ =
      [[RTCAudioSessionDelegateAdapter alloc] initWithObserver:this];
}

AudioDeviceIOS::~AudioDeviceIOS() {
  LOGI() << "~dtor" << ios::GetCurrentThreadDescription();
  audio_session_observer_ = nil;
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
}

int32_t AudioDeviceIOS::Init() {
  LOGI() << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (initialized_) {
    return 0;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif
  // Store the preferred sample rate and preferred number of channels already
  // here. They have not been set and confirmed yet since configureForWebRTC
  // is not called until audio is about to start. However, it makes sense to
  // store the parameters now and then verify at a later stage.
  RTCAudioSessionConfiguration* config =
      [RTCAudioSessionConfiguration webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate,
                            config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate,
                           config.inputNumberOfChannels);
  // Ensure that the audio device buffer (ADB) knows about the internal audio
  // parameters. Note that, even if we are unable to get a mono audio session,
  // we will always tell the I/O audio unit to do a channel format conversion
  // to guarantee mono on the "input side" of the audio unit.
  UpdateAudioDeviceBuffer();
  initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    return 0;
  }
  StopPlayout();
  StopRecording();
  initialized_ = false;
  return 0;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!play_is_initialized_);
  RTC_DCHECK(!playing_);
  if (!rec_is_initialized_) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitPlayout!";
      return -1;
    }
  }
  play_is_initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!rec_is_initialized_);
  RTC_DCHECK(!recording_);
  if (!play_is_initialized_) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitRecording!";
      return -1;
    }
  }
  rec_is_initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(play_is_initialized_);
  RTC_DCHECK(!playing_);
  fine_audio_buffer_->ResetPlayout();
  if (!recording_) {
    OSStatus result = AudioOutputUnitStart(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed for StartPlayout: "
                      << result;
      return -1;
    }
    LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&playing_, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!play_is_initialized_ || !playing_) {
    return 0;
  }
  if (!recording_) {
    ShutdownPlayOrRecord();
  }
  play_is_initialized_ = false;
  rtc::AtomicOps::ReleaseStore(&playing_, 0);
  return 0;
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(rec_is_initialized_);
  RTC_DCHECK(!recording_);
  fine_audio_buffer_->ResetRecord();
  if (!playing_) {
    OSStatus result = AudioOutputUnitStart(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed for StartRecording: "
                      << result;
      return -1;
    }
    LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&recording_, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!rec_is_initialized_ || !recording_) {
    return 0;
  }
  if (!playing_) {
    ShutdownPlayOrRecord();
  }
  rec_is_initialized_ = false;
  rtc::AtomicOps::ReleaseStore(&recording_, 0);
  return 0;
}

// Change the default receiver playout route to speaker.
int32_t AudioDeviceIOS::SetLoudspeakerStatus(bool enable) {
  LOGI() << "SetLoudspeakerStatus(" << enable << ")";

  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  [session lockForConfiguration];
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
  [session unlockForConfiguration];
  return (error == nil) ? 0 : -1;
}

int32_t AudioDeviceIOS::GetLoudspeakerStatus(bool& enabled) const {
  LOGI() << "GetLoudspeakerStatus";
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
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
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  *params = playout_parameters_;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  LOGI() << "GetRecordAudioParameters";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  *params = record_parameters_;
  return 0;
}

void AudioDeviceIOS::OnInterruptionBegin() {
  RTC_DCHECK(async_invoker_);
  RTC_DCHECK(thread_);
  if (thread_->IsCurrent()) {
    HandleInterruptionBegin();
    return;
  }
  async_invoker_->AsyncInvoke<void>(
      thread_,
      rtc::Bind(&webrtc::AudioDeviceIOS::HandleInterruptionBegin, this));
}

void AudioDeviceIOS::OnInterruptionEnd() {
  RTC_DCHECK(async_invoker_);
  RTC_DCHECK(thread_);
  if (thread_->IsCurrent()) {
    HandleInterruptionEnd();
    return;
  }
  async_invoker_->AsyncInvoke<void>(
      thread_,
      rtc::Bind(&webrtc::AudioDeviceIOS::HandleInterruptionEnd, this));
}

void AudioDeviceIOS::OnValidRouteChange() {
  RTC_DCHECK(async_invoker_);
  RTC_DCHECK(thread_);
  if (thread_->IsCurrent()) {
    HandleValidRouteChange();
    return;
  }
  async_invoker_->AsyncInvoke<void>(
      thread_,
      rtc::Bind(&webrtc::AudioDeviceIOS::HandleValidRouteChange, this));
}

void AudioDeviceIOS::HandleInterruptionBegin() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTCLog(@"Stopping the audio unit due to interruption begin.");
  LOG_IF_ERROR(AudioOutputUnitStop(vpio_unit_),
               "Failed to stop the the Voice-Processing I/O unit");
  is_interrupted_ = true;
}

void AudioDeviceIOS::HandleInterruptionEnd() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTCLog(@"Starting the audio unit due to interruption end.");
  LOG_IF_ERROR(AudioOutputUnitStart(vpio_unit_),
               "Failed to start the the Voice-Processing I/O unit");
  is_interrupted_ = false;
}

void AudioDeviceIOS::HandleValidRouteChange() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  // Don't do anything if we're interrupted.
  if (is_interrupted_) {
    return;
  }

  // Only restart audio for a valid route change if the session sample rate
  // has changed.
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  const double current_sample_rate = playout_parameters_.sample_rate();
  const double session_sample_rate = session.sampleRate;
  if (current_sample_rate != session_sample_rate) {
    RTCLog(@"Route changed caused sample rate to change from %f to %f. "
           "Restarting audio unit.", current_sample_rate, session_sample_rate);
    if (!RestartAudioUnitWithNewFormat(session_sample_rate)) {
      RTCLogError(@"Audio restart failed.");
    }
  }
}

void AudioDeviceIOS::UpdateAudioDeviceBuffer() {
  LOGI() << "UpdateAudioDevicebuffer";
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  // Inform the audio device buffer (ADB) about the new audio format.
  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  audio_device_buffer_->SetRecordingSampleRate(
      record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
}

void AudioDeviceIOS::SetupAudioBuffersForActiveAudioSession() {
  LOGI() << "SetupAudioBuffersForActiveAudioSession";
  // Verify the current values once the audio session has been activated.
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  double sample_rate = session.sampleRate;
  NSTimeInterval io_buffer_duration = session.IOBufferDuration;
  LOG(LS_INFO) << " sample rate: " << sample_rate;
  LOG(LS_INFO) << " IO buffer duration: " << io_buffer_duration;
  LOG(LS_INFO) << " output channels: " << session.outputNumberOfChannels;
  LOG(LS_INFO) << " input channels: " << session.inputNumberOfChannels;
  LOG(LS_INFO) << " output latency: " << session.outputLatency;
  LOG(LS_INFO) << " input latency: " << session.inputLatency;

  // Log a warning message for the case when we are unable to set the preferred
  // hardware sample rate but continue and use the non-ideal sample rate after
  // reinitializing the audio parameters. Most BT headsets only support 8kHz or
  // 16kHz.
  RTCAudioSessionConfiguration* webRTCConfig =
      [RTCAudioSessionConfiguration webRTCConfiguration];
  if (sample_rate != webRTCConfig.sampleRate) {
    LOG(LS_WARNING) << "Unable to set the preferred sample rate";
  }

  // At this stage, we also know the exact IO buffer duration and can add
  // that info to the existing audio parameters where it is converted into
  // number of audio frames.
  // Example: IO buffer size = 0.008 seconds <=> 128 audio frames at 16kHz.
  // Hence, 128 is the size we expect to see in upcoming render callbacks.
  playout_parameters_.reset(sample_rate, playout_parameters_.channels(),
                            io_buffer_duration);
  RTC_DCHECK(playout_parameters_.is_complete());
  record_parameters_.reset(sample_rate, record_parameters_.channels(),
                           io_buffer_duration);
  RTC_DCHECK(record_parameters_.is_complete());
  LOG(LS_INFO) << " frames per I/O buffer: "
               << playout_parameters_.frames_per_buffer();
  LOG(LS_INFO) << " bytes per I/O buffer: "
               << playout_parameters_.GetBytesPerBuffer();
  RTC_DCHECK_EQ(playout_parameters_.GetBytesPerBuffer(),
                record_parameters_.GetBytesPerBuffer());

  // Update the ADB parameters since the sample rate might have changed.
  UpdateAudioDeviceBuffer();

  // Create a modified audio buffer class which allows us to ask for,
  // or deliver, any number of samples (and not only multiple of 10ms) to match
  // the native audio unit buffer size.
  RTC_DCHECK(audio_device_buffer_);
  fine_audio_buffer_.reset(new FineAudioBuffer(
      audio_device_buffer_, playout_parameters_.GetBytesPerBuffer(),
      playout_parameters_.sample_rate()));

  // The extra/temporary playoutbuffer must be of this size to avoid
  // unnecessary memcpy while caching data between successive callbacks.
  const int required_playout_buffer_size =
      fine_audio_buffer_->RequiredPlayoutBufferSizeBytes();
  LOG(LS_INFO) << " required playout buffer size: "
               << required_playout_buffer_size;
  playout_audio_buffer_.reset(new SInt8[required_playout_buffer_size]);

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer
  // in |record_audio_buffer_|. Recorded audio will be rendered into this memory
  // at each input callback when calling AudioUnitRender().
  const int data_byte_size = record_parameters_.GetBytesPerBuffer();
  record_audio_buffer_.reset(new SInt8[data_byte_size]);
  audio_record_buffer_list_.mNumberBuffers = 1;
  AudioBuffer* audio_buffer = &audio_record_buffer_list_.mBuffers[0];
  audio_buffer->mNumberChannels = record_parameters_.channels();
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = record_audio_buffer_.get();
}

bool AudioDeviceIOS::SetupAndInitializeVoiceProcessingAudioUnit() {
  LOGI() << "SetupAndInitializeVoiceProcessingAudioUnit";
  RTC_DCHECK(!vpio_unit_) << "VoiceProcessingIO audio unit already exists";
  // Create an audio component description to identify the Voice-Processing
  // I/O audio unit.
  AudioComponentDescription vpio_unit_description;
  vpio_unit_description.componentType = kAudioUnitType_Output;
  vpio_unit_description.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
  vpio_unit_description.componentManufacturer = kAudioUnitManufacturer_Apple;
  vpio_unit_description.componentFlags = 0;
  vpio_unit_description.componentFlagsMask = 0;

  // Obtain an audio unit instance given the description.
  AudioComponent found_vpio_unit_ref =
      AudioComponentFindNext(nullptr, &vpio_unit_description);

  // Create a Voice-Processing IO audio unit.
  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_vpio_unit_ref, &vpio_unit_);
  if (result != noErr) {
    vpio_unit_ = nullptr;
    LOG(LS_ERROR) << "AudioComponentInstanceNew failed: " << result;
    return false;
  }

  // A VP I/O unit's bus 1 connects to input hardware (microphone). Enable
  // input on the input scope of the input element.
  AudioUnitElement input_bus = 1;
  UInt32 enable_input = 1;
  result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input, input_bus, &enable_input,
                                sizeof(enable_input));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to enable input on input scope of input element: "
                  << result;
    return false;
  }

  // A VP I/O unit's bus 0 connects to output hardware (speaker). Enable
  // output on the output scope of the output element.
  AudioUnitElement output_bus = 0;
  UInt32 enable_output = 1;
  result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, output_bus,
                                &enable_output, sizeof(enable_output));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to enable output on output scope of output element: "
        << result;
    return false;
  }

  // Set the application formats for input and output:
  // - use same format in both directions
  // - avoid resampling in the I/O unit by using the hardware sample rate
  // - linear PCM => noncompressed audio data format with one frame per packet
  // - no need to specify interleaving since only mono is supported
  AudioStreamBasicDescription application_format = {0};
  UInt32 size = sizeof(application_format);
  RTC_DCHECK_EQ(playout_parameters_.sample_rate(),
                record_parameters_.sample_rate());
  RTC_DCHECK_EQ(1, kRTCAudioSessionPreferredNumberOfChannels);
  application_format.mSampleRate = playout_parameters_.sample_rate();
  application_format.mFormatID = kAudioFormatLinearPCM;
  application_format.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  application_format.mBytesPerPacket = kBytesPerSample;
  application_format.mFramesPerPacket = 1;  // uncompressed
  application_format.mBytesPerFrame = kBytesPerSample;
  application_format.mChannelsPerFrame =
      kRTCAudioSessionPreferredNumberOfChannels;
  application_format.mBitsPerChannel = 8 * kBytesPerSample;
  // Store the new format.
  application_format_ = application_format;
#if !defined(NDEBUG)
  LogABSD(application_format_);
#endif

  // Set the application format on the output scope of the input element/bus.
  result = AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, input_bus,
                                &application_format, size);
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to set application format on output scope of input bus: "
        << result;
    return false;
  }

  // Set the application format on the input scope of the output element/bus.
  result = AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, output_bus,
                                &application_format, size);
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to set application format on input scope of output bus: "
        << result;
    return false;
  }

  // Specify the callback function that provides audio samples to the audio
  // unit.
  AURenderCallbackStruct render_callback;
  render_callback.inputProc = GetPlayoutData;
  render_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      vpio_unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
      output_bus, &render_callback, sizeof(render_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to specify the render callback on the output bus: "
                  << result;
    return false;
  }

  // Disable AU buffer allocation for the recorder, we allocate our own.
  // TODO(henrika): not sure that it actually saves resource to make this call.
  UInt32 flag = 0;
  result = AudioUnitSetProperty(
      vpio_unit_, kAudioUnitProperty_ShouldAllocateBuffer,
      kAudioUnitScope_Output, input_bus, &flag, sizeof(flag));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to disable buffer allocation on the input bus: "
                  << result;
  }

  // Specify the callback to be called by the I/O thread to us when input audio
  // is available. The recorded samples can then be obtained by calling the
  // AudioUnitRender() method.
  AURenderCallbackStruct input_callback;
  input_callback.inputProc = RecordedDataIsAvailable;
  input_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(vpio_unit_,
                                kAudioOutputUnitProperty_SetInputCallback,
                                kAudioUnitScope_Global, input_bus,
                                &input_callback, sizeof(input_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to specify the input callback on the input bus: "
                  << result;
  }

  // Initialize the Voice-Processing I/O unit instance.
  // Calls to AudioUnitInitialize() can fail if called back-to-back on
  // different ADM instances. The error message in this case is -66635 which is
  // undocumented. Tests have shown that calling AudioUnitInitialize a second
  // time, after a short sleep, avoids this issue.
  // See webrtc:5166 for details.
  int failed_initalize_attempts = 0;
  result = AudioUnitInitialize(vpio_unit_);
  while (result != noErr) {
    LOG(LS_ERROR) << "Failed to initialize the Voice-Processing I/O unit: "
                  << result;
    ++failed_initalize_attempts;
    if (failed_initalize_attempts == kMaxNumberOfAudioUnitInitializeAttempts) {
      // Max number of initialization attempts exceeded, hence abort.
      LOG(LS_WARNING) << "Too many initialization attempts";
      DisposeAudioUnit();
      return false;
    }
    LOG(LS_INFO) << "pause 100ms and try audio unit initialization again...";
    [NSThread sleepForTimeInterval:0.1f];
    result = AudioUnitInitialize(vpio_unit_);
  }
  LOG(LS_INFO) << "Voice-Processing I/O unit is now initialized";
  return true;
}

bool AudioDeviceIOS::RestartAudioUnitWithNewFormat(float sample_rate) {
  LOGI() << "RestartAudioUnitWithNewFormat(sample_rate=" << sample_rate << ")";
  // Stop the active audio unit.
  LOG_AND_RETURN_IF_ERROR(AudioOutputUnitStop(vpio_unit_),
                          "Failed to stop the the Voice-Processing I/O unit");

  // The stream format is about to be changed and it requires that we first
  // uninitialize it to deallocate its resources.
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitUninitialize(vpio_unit_),
      "Failed to uninitialize the the Voice-Processing I/O unit");

  // Allocate new buffers given the new stream format.
  SetupAudioBuffersForActiveAudioSession();

  // Update the existing application format using the new sample rate.
  application_format_.mSampleRate = playout_parameters_.sample_rate();
  UInt32 size = sizeof(application_format_);
  AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Output, 1, &application_format_, size);
  AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &application_format_, size);

  // Prepare the audio unit to render audio again.
  LOG_AND_RETURN_IF_ERROR(AudioUnitInitialize(vpio_unit_),
                          "Failed to initialize the Voice-Processing I/O unit");
  LOG(LS_INFO) << "Voice-Processing I/O unit is now reinitialized";

  // Start rendering audio using the new format.
  LOG_AND_RETURN_IF_ERROR(AudioOutputUnitStart(vpio_unit_),
                          "Failed to start the Voice-Processing I/O unit");
  LOG(LS_INFO) << "Voice-Processing I/O unit is now restarted";
  return true;
}

bool AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";

  // Use the correct audio session configuration for WebRTC.
  // This will attempt to activate the audio session.
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  [session lockForConfiguration];
  NSError* error = nil;
  if (![session configureWebRTCSession:&error]) {
    RTCLogError(@"Failed to configure WebRTC session: %@",
                error.localizedDescription);
    [session unlockForConfiguration];
    return false;
  }

  // Start observing audio session interruptions and route changes.
  [session pushDelegate:audio_session_observer_];

  // Ensure that we got what what we asked for in our active audio session.
  SetupAudioBuffersForActiveAudioSession();

  // Create, setup and initialize a new Voice-Processing I/O unit.
  if (!SetupAndInitializeVoiceProcessingAudioUnit()) {
    [session setActive:NO error:nil];
    [session unlockForConfiguration];
    return false;
  }
  [session unlockForConfiguration];
  return true;
}

void AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
  // Close and delete the voice-processing I/O unit.
  OSStatus result = -1;
  if (nullptr != vpio_unit_) {
    result = AudioOutputUnitStop(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStop failed: " << result;
    }
    result = AudioUnitUninitialize(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioUnitUninitialize failed: " << result;
    }
    DisposeAudioUnit();
  }

  // Remove audio session notification observers.
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  [session removeDelegate:audio_session_observer_];

  // All I/O should be stopped or paused prior to deactivating the audio
  // session, hence we deactivate as last action.
  [session lockForConfiguration];
  [session setActive:NO error:nil];
  [session unlockForConfiguration];
}

void AudioDeviceIOS::DisposeAudioUnit() {
  if (nullptr == vpio_unit_)
    return;
  OSStatus result = AudioComponentInstanceDispose(vpio_unit_);
  if (result != noErr) {
    LOG(LS_ERROR) << "AudioComponentInstanceDispose failed:" << result;
  }
  vpio_unit_ = nullptr;
}

OSStatus AudioDeviceIOS::RecordedDataIsAvailable(
    void* in_ref_con,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  RTC_DCHECK_EQ(1u, in_bus_number);
  RTC_DCHECK(
      !io_data);  // no buffer should be allocated for input at this stage
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(in_ref_con);
  return audio_device_ios->OnRecordedDataIsAvailable(
      io_action_flags, in_time_stamp, in_bus_number, in_number_frames);
}

OSStatus AudioDeviceIOS::OnRecordedDataIsAvailable(
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames) {
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&recording_))
    return result;
  if (in_number_frames != record_parameters_.frames_per_buffer()) {
    // We have seen short bursts (1-2 frames) where |in_number_frames| changes.
    // Add a log to keep track of longer sequences if that should ever happen.
    // Also return since calling AudioUnitRender in this state will only result
    // in kAudio_ParamError (-50) anyhow.
    LOG(LS_WARNING) << "in_number_frames (" << in_number_frames
                    << ") != " << record_parameters_.frames_per_buffer();
    return noErr;
  }
  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the |io_data| parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // TODO(henrika): should error handling be improved?
  AudioBufferList* io_data = &audio_record_buffer_list_;
  result = AudioUnitRender(vpio_unit_, io_action_flags, in_time_stamp,
                           in_bus_number, in_number_frames, io_data);
  if (result != noErr) {
    LOG_F(LS_ERROR) << "AudioUnitRender failed: " << result;
    return result;
  }
  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  const UInt32 data_size_in_bytes = io_data->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(data_size_in_bytes / kBytesPerSample, in_number_frames);
  SInt8* data = static_cast<SInt8*>(io_data->mBuffers[0].mData);
  fine_audio_buffer_->DeliverRecordedData(data, data_size_in_bytes,
                                          kFixedPlayoutDelayEstimate,
                                          kFixedRecordDelayEstimate);
  return noErr;
}

OSStatus AudioDeviceIOS::GetPlayoutData(
    void* in_ref_con,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  RTC_DCHECK_EQ(0u, in_bus_number);
  RTC_DCHECK(io_data);
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(in_ref_con);
  return audio_device_ios->OnGetPlayoutData(io_action_flags, in_number_frames,
                                            io_data);
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(
    AudioUnitRenderActionFlags* io_action_flags,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  // Verify 16-bit, noninterleaved mono PCM signal format.
  RTC_DCHECK_EQ(1u, io_data->mNumberBuffers);
  RTC_DCHECK_EQ(1u, io_data->mBuffers[0].mNumberChannels);
  // Get pointer to internal audio buffer to which new audio data shall be
  // written.
  const UInt32 dataSizeInBytes = io_data->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(dataSizeInBytes / kBytesPerSample, in_number_frames);
  SInt8* destination = static_cast<SInt8*>(io_data->mBuffers[0].mData);
  // Produce silence and give audio unit a hint about it if playout is not
  // activated.
  if (!rtc::AtomicOps::AcquireLoad(&playing_)) {
    *io_action_flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(destination, 0, dataSizeInBytes);
    return noErr;
  }
  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) to a preallocated intermediate buffer and
  // copy the result to the audio buffer in the |io_data| destination.
  SInt8* source = playout_audio_buffer_.get();
  fine_audio_buffer_->GetPlayoutData(source);
  memcpy(destination, source, dataSizeInBytes);
  return noErr;
}

}  // namespace webrtc
