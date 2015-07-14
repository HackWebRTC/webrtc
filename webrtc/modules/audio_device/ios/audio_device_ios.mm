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
#include "webrtc/modules/utility/interface/helpers_ios.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

#define LOGI() LOG(LS_INFO) << "AudioDeviceIOS::"

using ios::CheckAndLogError;

#if !defined(NDEBUG)
static void LogDeviceInfo() {
  LOG(LS_INFO) << "LogDeviceInfo";
  @autoreleasepool {
    LOG(LS_INFO) << " system name: " << ios::GetSystemName();
    LOG(LS_INFO) << " system version: " << ios::GetSystemVersion();
    LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
  }
}
#endif

static void ActivateAudioSession(AVAudioSession* session, bool activate) {
  LOG(LS_INFO) << "ActivateAudioSession(" << activate << ")";
  @autoreleasepool {
    NSError* error = nil;
    BOOL success = NO;
    if (!activate) {
      // Deactivate the audio session.
      success = [session setActive:NO error:&error];
      DCHECK(CheckAndLogError(success, error));
      return;
    }
    // Activate an audio session and set category and mode. Only make changes
    // if needed since setting them to the value they already have will clear
    // transient properties (such as PortOverride) that some other component
    // have set up.
    if (session.category != AVAudioSessionCategoryPlayAndRecord) {
      error = nil;
      success = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                               error:&error];
      DCHECK(CheckAndLogError(success, error));
    }
    if (session.mode != AVAudioSessionModeVoiceChat) {
      error = nil;
      success = [session setMode:AVAudioSessionModeVoiceChat error:&error];
      DCHECK(CheckAndLogError(success, error));
    }
    error = nil;
    success = [session setActive:YES error:&error];
    DCHECK(CheckAndLogError(success, error));
    // Ensure that category and mode are actually activated.
    DCHECK(
        [session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]);
    DCHECK([session.mode isEqualToString:AVAudioSessionModeVoiceChat]);
  }
}

// Query hardware characteristics, such as input and output latency, input and
// output channel count, hardware sample rate, hardware volume setting, and
// whether audio input is available. To obtain meaningful values for hardware
// characteristics,the audio session must be initialized and active before we
// query the values.
// TODO(henrika): Note that these characteristics can change at runtime. For
// instance, input sample rate may change when a user plugs in a headset.
static void GetHardwareAudioParameters(AudioParameters* playout_parameters,
                                       AudioParameters* record_parameters) {
  LOG(LS_INFO) << "GetHardwareAudioParameters";
  @autoreleasepool {
    // Implicit initialization happens when we obtain a reference to the
    // AVAudioSession object.
    AVAudioSession* session = [AVAudioSession sharedInstance];
    // Always get values when the audio session is active.
    ActivateAudioSession(session, true);
    CHECK(session.isInputAvailable) << "No input path is available!";
    // Get current hardware parameters.
    double sample_rate = (double)session.sampleRate;
    double io_buffer_duration = (double)session.IOBufferDuration;
    int output_channels = (int)session.outputNumberOfChannels;
    int input_channels = (int)session.inputNumberOfChannels;
    int frames_per_buffer =
        static_cast<int>(sample_rate * io_buffer_duration + 0.5);
    // Copy hardware parameters to output parameters.
    playout_parameters->reset(sample_rate, output_channels, frames_per_buffer);
    record_parameters->reset(sample_rate, input_channels, frames_per_buffer);
    // Add logging for debugging purposes.
    LOG(LS_INFO) << " sample rate: " << sample_rate;
    LOG(LS_INFO) << " IO buffer duration: " << io_buffer_duration;
    LOG(LS_INFO) << " frames_per_buffer: " << frames_per_buffer;
    LOG(LS_INFO) << " output channels: " << output_channels;
    LOG(LS_INFO) << " input channels: " << input_channels;
    LOG(LS_INFO) << " output latency: " << (double)session.outputLatency;
    LOG(LS_INFO) << " input latency: " << (double)session.inputLatency;
    // Don't keep the audio session active. Instead, deactivate when needed.
    ActivateAudioSession(session, false);
    // TODO(henrika): to be extra safe, we can do more here. E.g., set
    // preferred values for sample rate, channels etc., re-activate an audio
    // session and verify the actual values again. Then we know for sure that
    // the current values will in fact be correct. Or, we can skip all this
    // and check setting when audio is started. Probably better.
  }
}

AudioDeviceIOS::AudioDeviceIOS()
    : audio_device_buffer_(nullptr),
      _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
      _auVoiceProcessing(nullptr),
      _audioInterruptionObserver(nullptr),
      _initialized(false),
      _isShutDown(false),
      _recording(false),
      _playing(false),
      _recIsInitialized(false),
      _playIsInitialized(false),
      _adbSampFreq(0),
      _recordingDelay(0),
      _playoutDelay(0),
      _playoutDelayMeasurementCounter(9999),
      _recordingDelayHWAndOS(0),
      _recordingDelayMeasurementCounter(9999),
      _playoutBufferUsed(0),
      _recordingCurrentSeq(0),
      _recordingBufferTotalSize(0) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription();
  memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
  memset(_recordingBuffer, 0, sizeof(_recordingBuffer));
  memset(_recordingLength, 0, sizeof(_recordingLength));
  memset(_recordingSeqNumber, 0, sizeof(_recordingSeqNumber));
}

AudioDeviceIOS::~AudioDeviceIOS() {
  LOGI() << "~dtor";
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  delete &_critSect;
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  DCHECK(audioBuffer);
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
}

int32_t AudioDeviceIOS::Init() {
  LOGI() << "Init";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (_initialized) {
    return 0;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif
  // Query hardware audio parameters and cache the results. These parameters
  // will be used as preferred values later when streaming starts.
  // Note that I override these "optimal" value below since I don't want to
  // modify the existing behavior yet.
  GetHardwareAudioParameters(&playout_parameters_, &record_parameters_);
  // TODO(henrika): these parameters are currently hard coded to match the
  // existing implementation where we always use 16kHz as preferred sample
  // rate and mono only. Goal is to improve this scheme and make it more
  // flexible. In addition, a better native buffer size shall be derived.
  // Using 10ms as default here (only used by unit test so far).
  // We should also implemented observers for notification of any change in
  // these parameters.
  playout_parameters_.reset(16000, 1, 160);
  record_parameters_.reset(16000, 1, 160);

  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  // Inform the audio device buffer (ADB) about the new audio format.
  // TODO(henrika): try to improve this section.
  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  audio_device_buffer_->SetRecordingSampleRate(
      record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());

  DCHECK(!_captureWorkerThread);
  // Create and start the capture thread.
  // TODO(henrika): do we need this thread?
  _isShutDown = false;
  _captureWorkerThread =
      ThreadWrapper::CreateThread(RunCapture, this, "CaptureWorkerThread");
  if (!_captureWorkerThread->Start()) {
    LOG_F(LS_ERROR) << "Failed to start CaptureWorkerThread!";
    return -1;
  }
  _captureWorkerThread->SetPriority(kRealtimePriority);
  _initialized = true;
  return 0;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!_initialized) {
    return 0;
  }
  // Stop the capture thread.
  if (_captureWorkerThread) {
    if (!_captureWorkerThread->Stop()) {
      LOG_F(LS_ERROR) << "Failed to stop CaptureWorkerThread!";
      return -1;
    }
    _captureWorkerThread.reset();
  }
  ShutdownPlayOrRecord();
  _isShutDown = true;
  _initialized = false;
  return 0;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(_initialized);
  DCHECK(!_playIsInitialized);
  DCHECK(!_playing);
  if (!_recIsInitialized) {
    if (InitPlayOrRecord() == -1) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed!";
      return -1;
    }
  }
  _playIsInitialized = true;
  return 0;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitPlayout";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(_initialized);
  DCHECK(!_recIsInitialized);
  DCHECK(!_recording);
  if (!_playIsInitialized) {
    if (InitPlayOrRecord() == -1) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed!";
      return -1;
    }
  }
  _recIsInitialized = true;
  return 0;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(_playIsInitialized);
  DCHECK(!_playing);

  CriticalSectionScoped lock(&_critSect);

  memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
  _playoutBufferUsed = 0;
  _playoutDelay = 0;
  // Make sure first call to update delay function will update delay
  _playoutDelayMeasurementCounter = 9999;

  if (!_recording) {
    OSStatus result = AudioOutputUnitStart(_auVoiceProcessing);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed: " << result;
      return -1;
    }
  }
  _playing = true;
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!_playIsInitialized || !_playing) {
    return 0;
  }

  CriticalSectionScoped lock(&_critSect);

  if (!_recording) {
    // Both playout and recording has stopped, shutdown the device.
    ShutdownPlayOrRecord();
  }
  _playIsInitialized = false;
  _playing = false;
  return 0;
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(_recIsInitialized);
  DCHECK(!_recording);

  CriticalSectionScoped lock(&_critSect);

  memset(_recordingBuffer, 0, sizeof(_recordingBuffer));
  memset(_recordingLength, 0, sizeof(_recordingLength));
  memset(_recordingSeqNumber, 0, sizeof(_recordingSeqNumber));

  _recordingCurrentSeq = 0;
  _recordingBufferTotalSize = 0;
  _recordingDelay = 0;
  _recordingDelayHWAndOS = 0;
  // Make sure first call to update delay function will update delay
  _recordingDelayMeasurementCounter = 9999;

  if (!_playing) {
    OSStatus result = AudioOutputUnitStart(_auVoiceProcessing);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed: " << result;
      return -1;
    }
  }
  _recording = true;
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!_recIsInitialized || !_recording) {
    return 0;
  }

  CriticalSectionScoped lock(&_critSect);

  if (!_playing) {
    // Both playout and recording has stopped, shutdown the device.
    ShutdownPlayOrRecord();
  }
  _recIsInitialized = false;
  _recording = false;
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
  delayMS = _playoutDelay;
  return 0;
}

int32_t AudioDeviceIOS::RecordingDelay(uint16_t& delayMS) const {
  delayMS = _recordingDelay;
  return 0;
}

int32_t AudioDeviceIOS::PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                      uint16_t& sizeMS) const {
  type = AudioDeviceModule::kAdaptiveBufferSize;
  sizeMS = _playoutDelay;
  return 0;
}

int AudioDeviceIOS::GetPlayoutAudioParameters(AudioParameters* params) const {
  CHECK(playout_parameters_.is_valid());
  DCHECK(thread_checker_.CalledOnValidThread());
  *params = playout_parameters_;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  CHECK(record_parameters_.is_valid());
  DCHECK(thread_checker_.CalledOnValidThread());
  *params = record_parameters_;
  return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

int32_t AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "AudioDeviceIOS::InitPlayOrRecord";
  DCHECK(!_auVoiceProcessing);

  OSStatus result = -1;

  // Create Voice Processing Audio Unit
  AudioComponentDescription desc;
  AudioComponent comp;

  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  comp = AudioComponentFindNext(nullptr, &desc);
  if (nullptr == comp) {
    LOG_F(LS_ERROR) << "Could not find audio component for Audio Unit";
    return -1;
  }

  result = AudioComponentInstanceNew(comp, &_auVoiceProcessing);
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to create Audio Unit instance: " << result;
    return -1;
  }

  // TODO(henrika): I think we should set the preferred channel configuration
  // in both directions as well to be safe.

  // Set preferred hardware sample rate to 16 kHz.
  // TODO(henrika): improve this selection of sample rate. Why do we currently
  // use a hard coded value? How can we fail and still continue?
  NSError* error = nil;
  AVAudioSession* session = [AVAudioSession sharedInstance];
  Float64 preferredSampleRate(playout_parameters_.sample_rate());
  [session setPreferredSampleRate:preferredSampleRate error:&error];
  if (error != nil) {
    const char* errorString = [[error localizedDescription] UTF8String];
    LOG_F(LS_ERROR) << "setPreferredSampleRate failed: " << errorString;
  }

  // TODO(henrika): we can reduce latency by setting the IOBufferDuration
  // here. Default size for 16kHz is 0.016 sec or 16 msec on an iPhone 6.

  // Activate the audio session.
  ActivateAudioSession(session, true);

  UInt32 enableIO = 1;
  result = AudioUnitSetProperty(_auVoiceProcessing,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                1,  // input bus
                                &enableIO, sizeof(enableIO));
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to enable IO on input: " << result;
  }

  result = AudioUnitSetProperty(_auVoiceProcessing,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                0,  // output bus
                                &enableIO, sizeof(enableIO));
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to enable IO on output: " << result;
  }

  // Disable AU buffer allocation for the recorder, we allocate our own.
  // TODO(henrika): understand this part better.
  UInt32 flag = 0;
  result = AudioUnitSetProperty(_auVoiceProcessing,
                                kAudioUnitProperty_ShouldAllocateBuffer,
                                kAudioUnitScope_Output, 1, &flag, sizeof(flag));
  if (0 != result) {
    LOG_F(LS_WARNING) << "Failed to disable AU buffer allocation: " << result;
    // Should work anyway
  }

  // Set recording callback.
  AURenderCallbackStruct auCbS;
  memset(&auCbS, 0, sizeof(auCbS));
  auCbS.inputProc = RecordProcess;
  auCbS.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      _auVoiceProcessing, kAudioOutputUnitProperty_SetInputCallback,
      kAudioUnitScope_Global, 1, &auCbS, sizeof(auCbS));
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to set AU record callback: " << result;
  }

  // Set playout callback.
  memset(&auCbS, 0, sizeof(auCbS));
  auCbS.inputProc = PlayoutProcess;
  auCbS.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      _auVoiceProcessing, kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Global, 0, &auCbS, sizeof(auCbS));
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to set AU output callback: " << result;
  }

  // Get stream format for out/0
  AudioStreamBasicDescription playoutDesc;
  UInt32 size = sizeof(playoutDesc);
  result =
      AudioUnitGetProperty(_auVoiceProcessing, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Output, 0, &playoutDesc, &size);
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to get AU output stream format: " << result;
  }

  playoutDesc.mSampleRate = preferredSampleRate;
  LOG(LS_INFO) << "Audio Unit playout opened in sampling rate: "
               << playoutDesc.mSampleRate;

  // Store the sampling frequency to use towards the Audio Device Buffer
  // todo: Add 48 kHz (increase buffer sizes). Other fs?
  // TODO(henrika): Figure out if we really need this complex handling.
  if ((playoutDesc.mSampleRate > 44090.0) &&
      (playoutDesc.mSampleRate < 44110.0)) {
    _adbSampFreq = 44100;
  } else if ((playoutDesc.mSampleRate > 15990.0) &&
             (playoutDesc.mSampleRate < 16010.0)) {
    _adbSampFreq = 16000;
  } else if ((playoutDesc.mSampleRate > 7990.0) &&
             (playoutDesc.mSampleRate < 8010.0)) {
    _adbSampFreq = 8000;
  } else {
    _adbSampFreq = 0;
    FATAL() << "Invalid sample rate";
  }

  // Set the audio device buffer sampling rates (use same for play and record).
  // TODO(henrika): this is not a good place to set these things up.
  DCHECK(audio_device_buffer_);
  DCHECK_EQ(_adbSampFreq, playout_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingSampleRate(_adbSampFreq);
  audio_device_buffer_->SetPlayoutSampleRate(_adbSampFreq);

  // Set stream format for out/0.
  playoutDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                             kLinearPCMFormatFlagIsPacked |
                             kLinearPCMFormatFlagIsNonInterleaved;
  playoutDesc.mBytesPerPacket = 2;
  playoutDesc.mFramesPerPacket = 1;
  playoutDesc.mBytesPerFrame = 2;
  playoutDesc.mChannelsPerFrame = 1;
  playoutDesc.mBitsPerChannel = 16;
  result =
      AudioUnitSetProperty(_auVoiceProcessing, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, 0, &playoutDesc, size);
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to set AU stream format for out/0";
  }

  // Get stream format for in/1.
  AudioStreamBasicDescription recordingDesc;
  size = sizeof(recordingDesc);
  result =
      AudioUnitGetProperty(_auVoiceProcessing, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, 1, &recordingDesc, &size);
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to get AU stream format for in/1";
  }

  recordingDesc.mSampleRate = preferredSampleRate;
  LOG(LS_INFO) << "Audio Unit recording opened in sampling rate: "
               << recordingDesc.mSampleRate;

  // Set stream format for out/1 (use same sampling frequency as for in/1).
  recordingDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                               kLinearPCMFormatFlagIsPacked |
                               kLinearPCMFormatFlagIsNonInterleaved;
  recordingDesc.mBytesPerPacket = 2;
  recordingDesc.mFramesPerPacket = 1;
  recordingDesc.mBytesPerFrame = 2;
  recordingDesc.mChannelsPerFrame = 1;
  recordingDesc.mBitsPerChannel = 16;
  result =
      AudioUnitSetProperty(_auVoiceProcessing, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Output, 1, &recordingDesc, size);
  if (0 != result) {
    LOG_F(LS_ERROR) << "Failed to set AU stream format for out/1";
  }

  // Initialize here already to be able to get/set stream properties.
  result = AudioUnitInitialize(_auVoiceProcessing);
  if (0 != result) {
    LOG_F(LS_ERROR) << "AudioUnitInitialize failed: " << result;
  }

  // Get hardware sample rate for logging (see if we get what we asked for).
  // TODO(henrika): what if we don't get what we ask for?
  double sampleRate = session.sampleRate;
  LOG(LS_INFO) << "Current HW sample rate is: " << sampleRate
               << ", ADB sample rate is: " << _adbSampFreq;
  LOG(LS_INFO) << "Current HW IO buffer size is: " <<
      [session IOBufferDuration];

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
                    AudioOutputUnitStop(_auVoiceProcessing);
                    AudioOutputUnitStart(_auVoiceProcessing);
                    break;
                  }
                }
              }];
  // Increment refcount on observer using ARC bridge. Instance variable is a
  // void* instead of an id because header is included in other pure C++
  // files.
  _audioInterruptionObserver = (__bridge_retained void*)observer;

  // Deactivate the audio session.
  ActivateAudioSession(session, false);

  return 0;
}

int32_t AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";

  if (_audioInterruptionObserver != nullptr) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    // Transfer ownership of observer back to ARC, which will dealloc the
    // observer once it exits this scope.
    id observer = (__bridge_transfer id)_audioInterruptionObserver;
    [center removeObserver:observer];
    _audioInterruptionObserver = nullptr;
  }

  // Close and delete AU.
  OSStatus result = -1;
  if (nullptr != _auVoiceProcessing) {
    result = AudioOutputUnitStop(_auVoiceProcessing);
    if (0 != result) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStop failed: " << result;
    }
    result = AudioComponentInstanceDispose(_auVoiceProcessing);
    if (0 != result) {
      LOG_F(LS_ERROR) << "AudioComponentInstanceDispose failed: " << result;
    }
    _auVoiceProcessing = nullptr;
  }

  return 0;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

OSStatus AudioDeviceIOS::RecordProcess(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {
  AudioDeviceIOS* ptrThis = static_cast<AudioDeviceIOS*>(inRefCon);
  return ptrThis->RecordProcessImpl(ioActionFlags, inTimeStamp, inBusNumber,
                                    inNumberFrames);
}

OSStatus AudioDeviceIOS::RecordProcessImpl(
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    uint32_t inBusNumber,
    uint32_t inNumberFrames) {
  // Setup some basic stuff
  // Use temp buffer not to lock up recording buffer more than necessary
  // todo: Make dataTmp a member variable with static size that holds
  //       max possible frames?
  int16_t* dataTmp = new int16_t[inNumberFrames];
  memset(dataTmp, 0, 2 * inNumberFrames);

  AudioBufferList abList;
  abList.mNumberBuffers = 1;
  abList.mBuffers[0].mData = dataTmp;
  abList.mBuffers[0].mDataByteSize = 2 * inNumberFrames;  // 2 bytes/sample
  abList.mBuffers[0].mNumberChannels = 1;

  // Get data from mic
  OSStatus res = AudioUnitRender(_auVoiceProcessing, ioActionFlags, inTimeStamp,
                                 inBusNumber, inNumberFrames, &abList);
  if (res != 0) {
    // TODO(henrika): improve error handling.
    delete[] dataTmp;
    return 0;
  }

  if (_recording) {
    // Insert all data in temp buffer into recording buffers
    // There is zero or one buffer partially full at any given time,
    // all others are full or empty
    // Full means filled with noSamp10ms samples.

    const unsigned int noSamp10ms = _adbSampFreq / 100;
    unsigned int dataPos = 0;
    uint16_t bufPos = 0;
    int16_t insertPos = -1;
    unsigned int nCopy = 0;  // Number of samples to copy

    while (dataPos < inNumberFrames) {
      // Loop over all recording buffers or
      // until we find the partially full buffer
      // First choice is to insert into partially full buffer,
      // second choice is to insert into empty buffer
      bufPos = 0;
      insertPos = -1;
      nCopy = 0;
      while (bufPos < N_REC_BUFFERS) {
        if ((_recordingLength[bufPos] > 0) &&
            (_recordingLength[bufPos] < noSamp10ms)) {
          // Found the partially full buffer
          insertPos = static_cast<int16_t>(bufPos);
          // Don't need to search more, quit loop
          bufPos = N_REC_BUFFERS;
        } else if ((-1 == insertPos) && (0 == _recordingLength[bufPos])) {
          // Found an empty buffer
          insertPos = static_cast<int16_t>(bufPos);
        }
        ++bufPos;
      }

      // Insert data into buffer
      if (insertPos > -1) {
        // We found a non-full buffer, copy data to it
        unsigned int dataToCopy = inNumberFrames - dataPos;
        unsigned int currentRecLen = _recordingLength[insertPos];
        unsigned int roomInBuffer = noSamp10ms - currentRecLen;
        nCopy = (dataToCopy < roomInBuffer ? dataToCopy : roomInBuffer);

        memcpy(&_recordingBuffer[insertPos][currentRecLen], &dataTmp[dataPos],
               nCopy * sizeof(int16_t));
        if (0 == currentRecLen) {
          _recordingSeqNumber[insertPos] = _recordingCurrentSeq;
          ++_recordingCurrentSeq;
        }
        _recordingBufferTotalSize += nCopy;
        // Has to be done last to avoid interrupt problems between threads.
        _recordingLength[insertPos] += nCopy;
        dataPos += nCopy;
      } else {
        // Didn't find a non-full buffer
        // TODO(henrika): improve error handling
        dataPos = inNumberFrames;  // Don't try to insert more
      }
    }
  }
  delete[] dataTmp;
  return 0;
}

OSStatus AudioDeviceIOS::PlayoutProcess(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {
  AudioDeviceIOS* ptrThis = static_cast<AudioDeviceIOS*>(inRefCon);
  return ptrThis->PlayoutProcessImpl(inNumberFrames, ioData);
}

OSStatus AudioDeviceIOS::PlayoutProcessImpl(uint32_t inNumberFrames,
                                            AudioBufferList* ioData) {
  int16_t* data = static_cast<int16_t*>(ioData->mBuffers[0].mData);
  unsigned int dataSizeBytes = ioData->mBuffers[0].mDataByteSize;
  unsigned int dataSize = dataSizeBytes / 2;  // Number of samples
  CHECK_EQ(dataSize, inNumberFrames);
  memset(data, 0, dataSizeBytes);  // Start with empty buffer

  // Get playout data from Audio Device Buffer

  if (_playing) {
    unsigned int noSamp10ms = _adbSampFreq / 100;
    // todo: Member variable and allocate when samp freq is determined
    int16_t* dataTmp = new int16_t[noSamp10ms];
    memset(dataTmp, 0, 2 * noSamp10ms);
    unsigned int dataPos = 0;
    int noSamplesOut = 0;
    unsigned int nCopy = 0;

    // First insert data from playout buffer if any
    if (_playoutBufferUsed > 0) {
      nCopy = (dataSize < _playoutBufferUsed) ? dataSize : _playoutBufferUsed;
      DCHECK_EQ(nCopy, _playoutBufferUsed);
      memcpy(data, _playoutBuffer, 2 * nCopy);
      dataPos = nCopy;
      memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
      _playoutBufferUsed = 0;
    }

    // Now get the rest from Audio Device Buffer.
    while (dataPos < dataSize) {
      // Update playout delay
      UpdatePlayoutDelay();

      // Ask for new PCM data to be played out using the AudioDeviceBuffer
      noSamplesOut = audio_device_buffer_->RequestPlayoutData(noSamp10ms);

      // Get data from Audio Device Buffer
      noSamplesOut = audio_device_buffer_->GetPlayoutData(
          reinterpret_cast<int8_t*>(dataTmp));
      CHECK_EQ(noSamp10ms, (unsigned int)noSamplesOut);

      // Insert as much as fits in data buffer
      nCopy =
          (dataSize - dataPos) > noSamp10ms ? noSamp10ms : (dataSize - dataPos);
      memcpy(&data[dataPos], dataTmp, 2 * nCopy);

      // Save rest in playout buffer if any
      if (nCopy < noSamp10ms) {
        memcpy(_playoutBuffer, &dataTmp[nCopy], 2 * (noSamp10ms - nCopy));
        _playoutBufferUsed = noSamp10ms - nCopy;
      }

      // Update loop/index counter, if we copied less than noSamp10ms
      // samples we shall quit loop anyway
      dataPos += noSamp10ms;
    }
    delete[] dataTmp;
  }
  return 0;
}

// TODO(henrika): can either be removed or simplified.
void AudioDeviceIOS::UpdatePlayoutDelay() {
  ++_playoutDelayMeasurementCounter;

  if (_playoutDelayMeasurementCounter >= 100) {
    // Update HW and OS delay every second, unlikely to change

    // Since this is eventually rounded to integral ms, add 0.5ms
    // here to get round-to-nearest-int behavior instead of
    // truncation.
    double totalDelaySeconds = 0.0005;

    // HW output latency
    AVAudioSession* session = [AVAudioSession sharedInstance];
    double latency = session.outputLatency;
    assert(latency >= 0);
    totalDelaySeconds += latency;

    // HW buffer duration
    double ioBufferDuration = session.IOBufferDuration;
    assert(ioBufferDuration >= 0);
    totalDelaySeconds += ioBufferDuration;

    // AU latency
    Float64 f64(0);
    UInt32 size = sizeof(f64);
    OSStatus result =
        AudioUnitGetProperty(_auVoiceProcessing, kAudioUnitProperty_Latency,
                             kAudioUnitScope_Global, 0, &f64, &size);
    if (0 != result) {
      LOG_F(LS_ERROR) << "AU latency error: " << result;
    }
    assert(f64 >= 0);
    totalDelaySeconds += f64;

    // To ms
    _playoutDelay = static_cast<uint32_t>(totalDelaySeconds / 1000);

    // Reset counter
    _playoutDelayMeasurementCounter = 0;
  }

  // todo: Add playout buffer?
}

void AudioDeviceIOS::UpdateRecordingDelay() {
  ++_recordingDelayMeasurementCounter;

  if (_recordingDelayMeasurementCounter >= 100) {
    // Update HW and OS delay every second, unlikely to change

    // Since this is eventually rounded to integral ms, add 0.5ms
    // here to get round-to-nearest-int behavior instead of
    // truncation.
    double totalDelaySeconds = 0.0005;

    // HW input latency
    AVAudioSession* session = [AVAudioSession sharedInstance];
    double latency = session.inputLatency;
    assert(latency >= 0);
    totalDelaySeconds += latency;

    // HW buffer duration
    double ioBufferDuration = session.IOBufferDuration;
    assert(ioBufferDuration >= 0);
    totalDelaySeconds += ioBufferDuration;

    // AU latency
    Float64 f64(0);
    UInt32 size = sizeof(f64);
    OSStatus result =
        AudioUnitGetProperty(_auVoiceProcessing, kAudioUnitProperty_Latency,
                             kAudioUnitScope_Global, 0, &f64, &size);
    if (0 != result) {
      LOG_F(LS_ERROR) << "AU latency error: " << result;
    }
    assert(f64 >= 0);
    totalDelaySeconds += f64;

    // To ms
    _recordingDelayHWAndOS = static_cast<uint32_t>(totalDelaySeconds / 1000);

    // Reset counter
    _recordingDelayMeasurementCounter = 0;
  }

  _recordingDelay = _recordingDelayHWAndOS;

  // ADB recording buffer size, update every time
  // Don't count the one next 10 ms to be sent, then convert samples => ms
  const uint32_t noSamp10ms = _adbSampFreq / 100;
  if (_recordingBufferTotalSize > noSamp10ms) {
    _recordingDelay +=
        (_recordingBufferTotalSize - noSamp10ms) / (_adbSampFreq / 1000);
  }
}

bool AudioDeviceIOS::RunCapture(void* ptrThis) {
  return static_cast<AudioDeviceIOS*>(ptrThis)->CaptureWorkerThread();
}

bool AudioDeviceIOS::CaptureWorkerThread() {
  if (_recording) {
    int bufPos = 0;
    unsigned int lowestSeq = 0;
    int lowestSeqBufPos = 0;
    bool foundBuf = true;
    const unsigned int noSamp10ms = _adbSampFreq / 100;

    while (foundBuf) {
      // Check if we have any buffer with data to insert
      // into the Audio Device Buffer,
      // and find the one with the lowest seq number
      foundBuf = false;
      for (bufPos = 0; bufPos < N_REC_BUFFERS; ++bufPos) {
        if (noSamp10ms == _recordingLength[bufPos]) {
          if (!foundBuf) {
            lowestSeq = _recordingSeqNumber[bufPos];
            lowestSeqBufPos = bufPos;
            foundBuf = true;
          } else if (_recordingSeqNumber[bufPos] < lowestSeq) {
            lowestSeq = _recordingSeqNumber[bufPos];
            lowestSeqBufPos = bufPos;
          }
        }
      }

      // Insert data into the Audio Device Buffer if found any
      if (foundBuf) {
        // Update recording delay
        UpdateRecordingDelay();

        // Set the recorded buffer
        audio_device_buffer_->SetRecordedBuffer(
            reinterpret_cast<int8_t*>(_recordingBuffer[lowestSeqBufPos]),
            _recordingLength[lowestSeqBufPos]);

        // Don't need to set the current mic level in ADB since we only
        // support digital AGC,
        // and besides we cannot get or set the IOS mic level anyway.

        // Set VQE info, use clockdrift == 0
        audio_device_buffer_->SetVQEData(_playoutDelay, _recordingDelay, 0);

        // Deliver recorded samples at specified sample rate, mic level
        // etc. to the observer using callback
        audio_device_buffer_->DeliverRecordedData();

        // Make buffer available
        _recordingSeqNumber[lowestSeqBufPos] = 0;
        _recordingBufferTotalSize -= _recordingLength[lowestSeqBufPos];
        // Must be done last to avoid interrupt problems between threads
        _recordingLength[lowestSeqBufPos] = 0;
      }
    }
  }

  {
    // Normal case
    // Sleep thread (5ms) to let other threads get to work
    // todo: Is 5 ms optimal? Sleep shorter if inserted into the Audio
    //       Device Buffer?
    timespec t;
    t.tv_sec = 0;
    t.tv_nsec = 5 * 1000 * 1000;
    nanosleep(&t, nullptr);
  }
  return true;
}

}  // namespace webrtc
