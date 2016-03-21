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


// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

using ios::CheckAndLogError;

#if !defined(NDEBUG)
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
      audio_unit_(nullptr),
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
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }
  if (!recording_ &&
      audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
    if (!audio_unit_->Start()) {
      RTCLogError(@"StartPlayout failed to start audio unit.");
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
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetRecord();
  }
  if (!playing_ &&
      audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
    if (!audio_unit_->Start()) {
      RTCLogError(@"StartRecording failed to start audio unit.");
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

void AudioDeviceIOS::OnConfiguredForWebRTC() {
  RTC_DCHECK(async_invoker_);
  RTC_DCHECK(thread_);
  if (thread_->IsCurrent()) {
    HandleValidRouteChange();
    return;
  }
  async_invoker_->AsyncInvoke<void>(
      thread_,
      rtc::Bind(&webrtc::AudioDeviceIOS::HandleConfiguredForWebRTC, this));
}

OSStatus AudioDeviceIOS::OnDeliverRecordedData(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList* /* io_data */) {
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&recording_))
    return result;

  size_t frames_per_buffer = record_parameters_.frames_per_buffer();
  if (num_frames != frames_per_buffer) {
    // We have seen short bursts (1-2 frames) where |in_number_frames| changes.
    // Add a log to keep track of longer sequences if that should ever happen.
    // Also return since calling AudioUnitRender in this state will only result
    // in kAudio_ParamError (-50) anyhow.
    RTCLogWarning(@"Expected %u frames but got %u",
                  static_cast<unsigned int>(frames_per_buffer),
                  static_cast<unsigned int>(num_frames));
    return result;
  }

  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the |io_data| parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // We can make the audio unit provide a buffer instead in io_data, but we
  // currently just use our own.
  // TODO(henrika): should error handling be improved?
  AudioBufferList* io_data = &audio_record_buffer_list_;
  result =
      audio_unit_->Render(flags, time_stamp, bus_number, num_frames, io_data);
  if (result != noErr) {
    RTCLogError(@"Failed to render audio.");
    return result;
  }

  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  AudioBuffer* audio_buffer = &io_data->mBuffers[0];
  const size_t size_in_bytes = audio_buffer->mDataByteSize;
  RTC_CHECK_EQ(size_in_bytes / VoiceProcessingAudioUnit::kBytesPerSample,
               num_frames);
  int8_t* data = static_cast<int8_t*>(audio_buffer->mData);
  fine_audio_buffer_->DeliverRecordedData(data, size_in_bytes,
                                          kFixedPlayoutDelayEstimate,
                                          kFixedRecordDelayEstimate);
  return noErr;
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                          const AudioTimeStamp* time_stamp,
                                          UInt32 bus_number,
                                          UInt32 num_frames,
                                          AudioBufferList* io_data) {
  // Verify 16-bit, noninterleaved mono PCM signal format.
  RTC_DCHECK_EQ(1u, io_data->mNumberBuffers);
  AudioBuffer* audio_buffer = &io_data->mBuffers[0];
  RTC_DCHECK_EQ(1u, audio_buffer->mNumberChannels);
  // Get pointer to internal audio buffer to which new audio data shall be
  // written.
  const size_t size_in_bytes = audio_buffer->mDataByteSize;
  RTC_CHECK_EQ(size_in_bytes / VoiceProcessingAudioUnit::kBytesPerSample,
               num_frames);
  int8_t* destination = reinterpret_cast<int8_t*>(audio_buffer->mData);
  // Produce silence and give audio unit a hint about it if playout is not
  // activated.
  if (!rtc::AtomicOps::AcquireLoad(&playing_)) {
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(destination, 0, size_in_bytes);
    return noErr;
  }
  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) to a preallocated intermediate buffer and
  // copy the result to the audio buffer in the |io_data| destination.
  int8_t* source = playout_audio_buffer_.get();
  fine_audio_buffer_->GetPlayoutData(source);
  memcpy(destination, source, size_in_bytes);
  return noErr;
}

void AudioDeviceIOS::HandleInterruptionBegin() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  RTCLog(@"Stopping the audio unit due to interruption begin.");
  if (!audio_unit_->Stop()) {
    RTCLogError(@"Failed to stop the audio unit.");
  }
  is_interrupted_ = true;
}

void AudioDeviceIOS::HandleInterruptionEnd() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  RTCLog(@"Starting the audio unit due to interruption end.");
  if (!audio_unit_->Start()) {
    RTCLogError(@"Failed to start the audio unit.");
  }
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
    if (!RestartAudioUnit(session_sample_rate)) {
      RTCLogError(@"Audio restart failed.");
    }
  }
}

void AudioDeviceIOS::HandleConfiguredForWebRTC() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  // If we're not initialized we don't need to do anything. Audio unit will
  // be initialized on initialization.
  if (!rec_is_initialized_ && !play_is_initialized_)
    return;

  // If we're initialized, we must have an audio unit.
  RTC_DCHECK(audio_unit_);

  // Use configured audio session's settings to set up audio device buffer.
  // TODO(tkchin): Use RTCAudioSessionConfiguration to pick up settings and
  // pass it along.
  SetupAudioBuffersForActiveAudioSession();

  // Initialize the audio unit. This will affect any existing audio playback.
  if (!audio_unit_->Initialize(playout_parameters_.sample_rate())) {
    RTCLogError(@"Failed to initialize audio unit after configuration.");
    return;
  }

  // If we haven't started playing or recording there's nothing more to do.
  if (!playing_ && !recording_)
    return;

  // We are in a play or record state, start the audio unit.
  if (!audio_unit_->Start()) {
    RTCLogError(@"Failed to start audio unit after configuration.");
    return;
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
  RTCLog(@"%@", session);

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

bool AudioDeviceIOS::CreateAudioUnit() {
  RTC_DCHECK(!audio_unit_);

  audio_unit_.reset(new VoiceProcessingAudioUnit(this));
  if (!audio_unit_->Init()) {
    audio_unit_.reset();
    return false;
  }

  return true;
}

bool AudioDeviceIOS::RestartAudioUnit(float sample_rate) {
  RTCLog(@"Restarting audio unit with new sample rate: %f", sample_rate);

  // Stop the active audio unit.
  if (!audio_unit_->Stop()) {
    RTCLogError(@"Failed to stop the audio unit.");
    return false;
  }

  // The stream format is about to be changed and it requires that we first
  // uninitialize it to deallocate its resources.
  if (!audio_unit_->Uninitialize()) {
    RTCLogError(@"Failed to uninitialize the audio unit.");
    return false;
  }

  // Allocate new buffers given the new stream format.
  SetupAudioBuffersForActiveAudioSession();

  // Initialize the audio unit again with the new sample rate.
  RTC_DCHECK_EQ(playout_parameters_.sample_rate(), sample_rate);
  if (!audio_unit_->Initialize(sample_rate)) {
    RTCLogError(@"Failed to initialize the audio unit with sample rate: %f",
                sample_rate);
    return false;
  }

  // Restart the audio unit.
  if (!audio_unit_->Start()) {
    RTCLogError(@"Failed to start audio unit.");
    return false;
  }
  RTCLog(@"Successfully restarted audio unit.");

  return true;
}

bool AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";

  if (!CreateAudioUnit()) {
    return false;
  }

  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  // Subscribe to audio session events.
  [session pushDelegate:audio_session_observer_];

  // Lock the session to make configuration changes.
  [session lockForConfiguration];
  NSError* error = nil;
  if (![session beginWebRTCSession:&error]) {
    [session unlockForConfiguration];
    RTCLogError(@"Failed to begin WebRTC session: %@",
                error.localizedDescription);
    return false;
  }

  // If we are already configured properly, we can initialize the audio unit.
  if (session.isConfiguredForWebRTC) {
    [session unlockForConfiguration];
    SetupAudioBuffersForActiveAudioSession();
    // Audio session has been marked ready for WebRTC so we can initialize the
    // audio unit now.
    audio_unit_->Initialize(playout_parameters_.sample_rate());
    return true;
  }

  // Release the lock.
  [session unlockForConfiguration];

  return true;
}

void AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";

  // Close and delete the voice-processing I/O unit.
  if (audio_unit_) {
    audio_unit_.reset();
  }

  // Remove audio session notification observers.
  RTCAudioSession* session = [RTCAudioSession sharedInstance];
  [session removeDelegate:audio_session_observer_];

  // All I/O should be stopped or paused prior to deactivating the audio
  // session, hence we deactivate as last action.
  [session lockForConfiguration];
  [session endWebRTCSession:nil];
  [session unlockForConfiguration];
}

}  // namespace webrtc
