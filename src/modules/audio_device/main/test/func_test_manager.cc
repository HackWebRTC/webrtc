/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <ctype.h>
#include <cassert>
#include <string.h>

#include "func_test_manager.h"
#include "testsupport/fileutils.h"

#include "../source/audio_device_config.h"
#include "../source/audio_device_impl.h"

#ifndef __GNUC__
// Disable warning message ('sprintf': name was marked as #pragma deprecated)
#pragma warning( disable : 4995 )
// Disable warning message 4996 ('scanf': This function or variable may be unsafe)
#pragma warning( disable : 4996 )
#endif

const char* RecordedMicrophoneFile = "recorded_microphone_mono_48.pcm";
const char* RecordedMicrophoneVolumeFile =
"recorded_microphone_volume_mono_48.pcm";
const char* RecordedMicrophoneMuteFile = "recorded_microphone_mute_mono_48.pcm";
const char* RecordedMicrophoneBoostFile =
"recorded_microphone_boost_mono_48.pcm";
const char* RecordedMicrophoneAGCFile = "recorded_microphone_AGC_mono_48.pcm";
const char* RecordedSpeakerFile = "recorded_speaker_48.pcm";

struct AudioPacket
{
    WebRtc_UWord8 dataBuffer[4 * 960];
    WebRtc_UWord16 nSamples;
    WebRtc_UWord16 nBytesPerSample;
    WebRtc_UWord8 nChannels;
    WebRtc_UWord32 samplesPerSec;
};

// Helper functions
#if !defined(MAC_IPHONE) && !defined(ANDROID)
char* GetFilename(char* filename)
{
    return filename;
}
const char* GetFilename(const char* filename)
{
    return filename;
}
char* GetResource(char* resource)
{
    return resource;
}
const char* GetResource(const char* resource)
{
    return resource;
}
#endif

namespace webrtc
{

AudioEventObserver::AudioEventObserver(AudioDeviceModule* audioDevice) :
    _audioDevice(audioDevice)
{
}
;

AudioEventObserver::~AudioEventObserver()
{
}
;

void AudioEventObserver::OnErrorIsReported(const ErrorCode error)
{
    TEST_LOG("\n[*** ERROR ***] => OnErrorIsReported(%d)\n \n", error);
    _error = error;
    // TEST(_audioDevice->StopRecording() == 0);
    // TEST(_audioDevice->StopPlayout() == 0);
}
;


void AudioEventObserver::OnWarningIsReported(const WarningCode warning)
{
    TEST_LOG("\n[*** WARNING ***] => OnWarningIsReported(%d)\n \n", warning);
    _warning = warning;
    //TEST(_audioDevice->StopRecording() == 0);
    //TEST(_audioDevice->StopPlayout() == 0);
}
;

AudioTransportImpl::AudioTransportImpl(AudioDeviceModule* audioDevice) :
    _audioDevice(audioDevice),
    _playFromFile(false),
    _fullDuplex(false),
    _speakerVolume(false),
    _speakerMute(false),
    _microphoneVolume(false),
    _microphoneMute(false),
    _microphoneBoost(false),
    _microphoneAGC(false),
    _loopBackMeasurements(false),
    _playFile(*FileWrapper::Create()),
    _recCount(0),
    _playCount(0),
    _audioList()
{
    _resampler.Reset(48000, 48000, kResamplerSynchronousStereo);
}
;

AudioTransportImpl::~AudioTransportImpl()
{
    _playFile.Flush();
    _playFile.CloseFile();
    delete &_playFile;

    while (!_audioList.Empty())
    {
        ListItem* item = _audioList.First();
        if (item)
        {
            AudioPacket* packet = static_cast<AudioPacket*> (item->GetItem());
            if (packet)
            {
                delete packet;
            }
        }
        _audioList.PopFront();
    }
}
;

// ----------------------------------------------------------------------------
//	AudioTransportImpl::SetFilePlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioTransportImpl::SetFilePlayout(bool enable,
                                                 const WebRtc_Word8* fileName)
{
    _playFromFile = enable;
    if (enable)
    {
        return (_playFile.OpenFile(fileName, true, true, false));
    } else
    {
        _playFile.Flush();
        return (_playFile.CloseFile());
    }
}
;

void AudioTransportImpl::SetFullDuplex(bool enable)
{
    _fullDuplex = enable;

    while (!_audioList.Empty())
    {
        ListItem* item = _audioList.First();
        if (item)
        {
            AudioPacket* packet = static_cast<AudioPacket*> (item->GetItem());
            if (packet)
            {
                delete packet;
            }
        }
        _audioList.PopFront();
    }
}
;

WebRtc_Word32 AudioTransportImpl::RecordedDataIsAvailable(
    const WebRtc_Word8* audioSamples,
    const WebRtc_UWord32 nSamples,
    const WebRtc_UWord8 nBytesPerSample,
    const WebRtc_UWord8 nChannels,
    const WebRtc_UWord32 samplesPerSec,
    const WebRtc_UWord32 totalDelayMS,
    const WebRtc_Word32 clockDrift,
    const WebRtc_UWord32 currentMicLevel,
    WebRtc_UWord32& newMicLevel)
{
    if (_fullDuplex && _audioList.GetSize() < 15)
    {
        AudioPacket* packet = new AudioPacket();
        memcpy(packet->dataBuffer, audioSamples, nSamples * nBytesPerSample);
        packet->nSamples = (WebRtc_UWord16) nSamples;
        packet->nBytesPerSample = nBytesPerSample;
        packet->nChannels = nChannels;
        packet->samplesPerSec = samplesPerSec;
        _audioList.PushBack(packet);
    }

    _recCount++;
    if (_recCount % 100 == 0)
    {
        bool addMarker(true);

        if (_loopBackMeasurements)
        {
            addMarker = false;
        }

        if (_microphoneVolume)
        {
            WebRtc_UWord32 maxVolume(0);
            WebRtc_UWord32 minVolume(0);
            WebRtc_UWord32 volume(0);
            WebRtc_UWord16 stepSize(0);
            TEST(_audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(_audioDevice->MinMicrophoneVolume(&minVolume) == 0);
            TEST(_audioDevice->MicrophoneVolumeStepSize(&stepSize) == 0);
            TEST(_audioDevice->MicrophoneVolume(&volume) == 0);
            if (volume == 0)
            {
                TEST_LOG("[0]");
                addMarker = false;
            }
            int stepScale = (int) ((maxVolume - minVolume) / (stepSize * 10));
            volume += (stepScale * stepSize);
            if (volume > maxVolume)
            {
                TEST_LOG("[MAX]");
                volume = 0;
                addMarker = false;
            }
            TEST(_audioDevice->SetMicrophoneVolume(volume) == 0);
        }

        if (_microphoneAGC)
        {
            WebRtc_UWord32 maxVolume(0);
            WebRtc_UWord32 minVolume(0);
            WebRtc_UWord16 stepSize(0);
            TEST(_audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(_audioDevice->MinMicrophoneVolume(&minVolume) == 0);
            TEST(_audioDevice->MicrophoneVolumeStepSize(&stepSize) == 0);
            // emulate real AGC (min->max->min->max etc.)
            if (currentMicLevel <= 1)
            {
                TEST_LOG("[MIN]");
                addMarker = false;
            }
            int stepScale = (int) ((maxVolume - minVolume) / (stepSize * 10));
            newMicLevel = currentMicLevel + (stepScale * stepSize);
            if (newMicLevel > maxVolume)
            {
                TEST_LOG("[MAX]");
                newMicLevel = 1; // set lowest (non-zero) AGC level
                addMarker = false;
            }
        }

        if (_microphoneMute && (_recCount % 500 == 0))
        {
            bool muted(false);
            TEST(_audioDevice->MicrophoneMute(&muted) == 0);
            muted = !muted;
            TEST(_audioDevice->SetMicrophoneMute(muted) == 0);
            if (muted)
            {
                TEST_LOG("[MUTE ON]");
                addMarker = false;
            } else
            {
                TEST_LOG("[MUTE OFF]");
                addMarker = false;
            }
        }

        if (_microphoneBoost && (_recCount % 500 == 0))
        {
            bool boosted(false);
            TEST(_audioDevice->MicrophoneBoost(&boosted) == 0);
            boosted = !boosted;
            TEST(_audioDevice->SetMicrophoneBoost(boosted) == 0);
            if (boosted)
            {
                TEST_LOG("[BOOST ON]");
                addMarker = false;
            } else
            {
                TEST_LOG("[BOOST OFF]");
                addMarker = false;
            }
        }

        if ((nChannels == 1) && addMarker)
        {
            // mono
            TEST_LOG("-");
        } else if ((nChannels == 2) && (nBytesPerSample == 2) && addMarker)
        {
            AudioDeviceModule::ChannelType
                chType(AudioDeviceModule::kChannelLeft);
            TEST(_audioDevice->RecordingChannel(&chType) == 0);
            if (chType == AudioDeviceModule::kChannelLeft)
                TEST_LOG("-|");
            else
                TEST_LOG("|-");
        } else if (addMarker)
        {
            // stereo
            TEST_LOG("--");
        }

        if (nChannels == 2 && nBytesPerSample == 2)
        {
            // TEST_LOG("=> emulated mono (one channel exctracted from stereo input)\n");
        }
    }

    return 0;
}


WebRtc_Word32 AudioTransportImpl::NeedMorePlayData(
    const WebRtc_UWord32 nSamples,
    const WebRtc_UWord8 nBytesPerSample,
    const WebRtc_UWord8 nChannels,
    const WebRtc_UWord32 samplesPerSec,
    WebRtc_Word8* audioSamples,
    WebRtc_UWord32& nSamplesOut)
{
    if (_fullDuplex)
    {
        if (_audioList.Empty())
        {
            // use zero stuffing when not enough data
            memset(audioSamples, 0, nBytesPerSample * nSamples);
        } else
        {
            ListItem* item = _audioList.First();
            AudioPacket* packet = static_cast<AudioPacket*> (item->GetItem());
            if (packet)
            {
                int ret(0);
                int lenOut(0);
                WebRtc_Word16 tmpBuf_96kHz[80 * 12];
                WebRtc_Word16* ptr16In = NULL;
                WebRtc_Word16* ptr16Out = NULL;

                const WebRtc_UWord16 nSamplesIn = packet->nSamples;
                const WebRtc_UWord8 nChannelsIn = packet->nChannels;
                const WebRtc_UWord32 samplesPerSecIn = packet->samplesPerSec;
                const WebRtc_UWord16 nBytesPerSampleIn =
                    packet->nBytesPerSample;

                WebRtc_Word32 fsInHz(samplesPerSecIn);
                WebRtc_Word32 fsOutHz(samplesPerSec);

                if (fsInHz == 44100)
                    fsInHz = 44000;

                if (fsOutHz == 44100)
                    fsOutHz = 44000;

                if (nChannelsIn == 2 && nBytesPerSampleIn == 4)
                {
                    // input is stereo => we will resample in stereo
                    ret = _resampler.ResetIfNeeded(fsInHz, fsOutHz,
                                                   kResamplerSynchronousStereo);
                    if (ret == 0)
                    {
                        if (nChannels == 2)
                        {
                            _resampler.Push(
                                (const WebRtc_Word16*) packet->dataBuffer,
                                2 * nSamplesIn,
                                (WebRtc_Word16*) audioSamples, 2
                                * nSamples, lenOut);
                        } else
                        {
                            _resampler.Push(
                                (const WebRtc_Word16*) packet->dataBuffer,
                                2 * nSamplesIn, tmpBuf_96kHz, 2
                                * nSamples, lenOut);

                            ptr16In = &tmpBuf_96kHz[0];
                            ptr16Out = (WebRtc_Word16*) audioSamples;

                            // do stereo -> mono
                            for (unsigned int i = 0; i < nSamples; i++)
                            {
                                *ptr16Out = *ptr16In; // use left channel
                                ptr16Out++;
                                ptr16In++;
                                ptr16In++;
                            }
                        }
                        assert(2*nSamples == (WebRtc_UWord32)lenOut);
                    } else
                    {
                        if (_playCount % 100 == 0)
                            TEST_LOG(
                                     "ERROR: unable to resample from %d to %d\n",
                                     samplesPerSecIn, samplesPerSec);
                    }
                } else
                {
                    // input is mono (can be "reduced from stereo" as well) =>
                    // we will resample in mono
                    ret = _resampler.ResetIfNeeded(fsInHz, fsOutHz,
                                                   kResamplerSynchronous);
                    if (ret == 0)
                    {
                        if (nChannels == 1)
                        {
                            _resampler.Push(
                                (const WebRtc_Word16*) packet->dataBuffer,
                                nSamplesIn,
                                (WebRtc_Word16*) audioSamples,
                                nSamples, lenOut);
                        } else
                        {
                            _resampler.Push(
                                (const WebRtc_Word16*) packet->dataBuffer,
                                nSamplesIn, tmpBuf_96kHz, nSamples,
                                lenOut);

                            ptr16In = &tmpBuf_96kHz[0];
                            ptr16Out = (WebRtc_Word16*) audioSamples;

                            // do mono -> stereo
                            for (unsigned int i = 0; i < nSamples; i++)
                            {
                                *ptr16Out = *ptr16In; // left
                                ptr16Out++;
                                *ptr16Out = *ptr16In; // right (same as left sample)
                                ptr16Out++;
                                ptr16In++;
                            }
                        }
                        assert(nSamples == (WebRtc_UWord32)lenOut);
                    } else
                    {
                        if (_playCount % 100 == 0)
                            TEST_LOG("ERROR: unable to resample from %d to %d\n",
                                     samplesPerSecIn, samplesPerSec);
                    }
                }
                nSamplesOut = nSamples;
                delete packet;
            }
            _audioList.PopFront();
        }
    } // if (_fullDuplex)

    if (_playFromFile && _playFile.Open())
    {
        WebRtc_Word16 fileBuf[480];

        // read mono-file
        WebRtc_Word32 len = _playFile.Read((WebRtc_Word8*) fileBuf, 2
            * nSamples);
        if (len != 2 * (WebRtc_Word32) nSamples)
        {
            _playFile.Rewind();
            _playFile.Read((WebRtc_Word8*) fileBuf, 2 * nSamples);
        }

        // convert to stero if required
        if (nChannels == 1)
        {
            memcpy(audioSamples, fileBuf, 2 * nSamples);
        } else
        {
            // mono sample from file is duplicated and sent to left and right
            // channels
            WebRtc_Word16* audio16 = (WebRtc_Word16*) audioSamples;
            for (unsigned int i = 0; i < nSamples; i++)
            {
                (*audio16) = fileBuf[i]; // left
                audio16++;
                (*audio16) = fileBuf[i]; // right
                audio16++;
            }
        }
    } // if (_playFromFile && _playFile.Open())

    _playCount++;

    if (_playCount % 100 == 0)
    {
        bool addMarker(true);

        if (_speakerVolume)
        {
            WebRtc_UWord32 maxVolume(0);
            WebRtc_UWord32 minVolume(0);
            WebRtc_UWord32 volume(0);
            WebRtc_UWord16 stepSize(0);
            TEST(_audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
            TEST(_audioDevice->MinSpeakerVolume(&minVolume) == 0);
            TEST(_audioDevice->SpeakerVolumeStepSize(&stepSize) == 0);
            TEST(_audioDevice->SpeakerVolume(&volume) == 0);
            if (volume == 0)
            {
                TEST_LOG("[0]");
                addMarker = false;
            }
            WebRtc_UWord32 step = (maxVolume - minVolume) / 10;
            step = (step < stepSize ? stepSize : step);
            volume += step;
            if (volume > maxVolume)
            {
                TEST_LOG("[MAX]");
                volume = 0;
                addMarker = false;
            }
            TEST(_audioDevice->SetSpeakerVolume(volume) == 0);
        }

        if (_speakerMute && (_playCount % 500 == 0))
        {
            bool muted(false);
            TEST(_audioDevice->SpeakerMute(&muted) == 0);
            muted = !muted;
            TEST(_audioDevice->SetSpeakerMute(muted) == 0);
            if (muted)
            {
                TEST_LOG("[MUTE ON]");
                addMarker = false;
            } else
            {
                TEST_LOG("[MUTE OFF]");
                addMarker = false;
            }
        }

        if (_loopBackMeasurements)
        {
            WebRtc_UWord16 recDelayMS(0);
            WebRtc_UWord16 playDelayMS(0);
            WebRtc_UWord32 nItemsInList(0);

            nItemsInList = _audioList.GetSize();
            TEST(_audioDevice->RecordingDelay(&recDelayMS) == 0);
            TEST(_audioDevice->PlayoutDelay(&playDelayMS) == 0);
            TEST_LOG("Delay (rec+play)+buf: %3u (%3u+%3u)+%3u [ms]\n",
                     recDelayMS + playDelayMS + 10 * (nItemsInList + 1),
                     recDelayMS, playDelayMS, 10 * (nItemsInList + 1));

            addMarker = false;
        }

        if ((nChannels == 1) && addMarker)
        {
            TEST_LOG("+");
        } else if ((nChannels == 2) && addMarker)
        {
            TEST_LOG("++");
        }
    } // if (_playCount % 100 == 0)

    nSamplesOut = nSamples;

    return 0;
}
;

FuncTestManager::FuncTestManager() :
    _resourcePath(webrtc::test::ProjectRootPath() +
        "test/data/audio_device/"),
    _processThread(NULL),
    _audioDevice(NULL),
    _audioEventObserver(NULL),
    _audioTransport(NULL)
{
  assert(!_resourcePath.empty());
  _playoutFile48 = _resourcePath + "audio_short48.pcm";
  _playoutFile44 = _resourcePath + "audio_short44.pcm";
  _playoutFile16 = _resourcePath + "audio_short16.pcm";
  _playoutFile8 = _resourcePath + "audio_short8.pcm";
}

FuncTestManager::~FuncTestManager()
{
}

WebRtc_Word32 FuncTestManager::Init()
{
    TEST((_processThread = ProcessThread::CreateProcessThread()) != NULL);
    if (_processThread == NULL)
    {
        return -1;
    }
    _processThread->Start();

    // create the Audio Device module
    TEST((_audioDevice = AudioDeviceModuleImpl::Create(
        555, ADM_AUDIO_LAYER)) != NULL);
    if (_audioDevice == NULL)
    {
        return -1;
    }
    TEST(_audioDevice->AddRef() == 1);

    // register the Audio Device module
    _processThread->RegisterModule(_audioDevice);

    // register event observer
    _audioEventObserver = new AudioEventObserver(_audioDevice);
    TEST(_audioDevice->RegisterEventObserver(_audioEventObserver) == 0);

    // register audio transport
    _audioTransport = new AudioTransportImpl(_audioDevice);
    TEST(_audioDevice->RegisterAudioCallback(_audioTransport) == 0);

    WebRtc_Word8 version[256];
    WebRtc_UWord32 remainingBufferInBytes = 256;
    WebRtc_UWord32 position = 0;

    // log version
    TEST(_audioDevice->Version(version, remainingBufferInBytes, position) == 0);
    TEST_LOG("Version: %s\n \n", version);

    return 0;
}

WebRtc_Word32 FuncTestManager::Close()
{
    TEST(_audioDevice->RegisterEventObserver(NULL) == 0);
    TEST(_audioDevice->RegisterAudioCallback(NULL) == 0);
    TEST(_audioDevice->Terminate() == 0);

    // release the ProcessThread object
    if (_processThread)
    {
        _processThread->DeRegisterModule(_audioDevice);
        _processThread->Stop();
        ProcessThread::DestroyProcessThread(_processThread);
    }

    // delete the audio observer
    if (_audioEventObserver)
    {
        delete _audioEventObserver;
        _audioEventObserver = NULL;
    }

    // delete the audio transport
    if (_audioTransport)
    {
        delete _audioTransport;
        _audioTransport = NULL;
    }

    // release the AudioDeviceModule object
    if (_audioDevice)
    {
        TEST(_audioDevice->Release() == 0);
        _audioDevice = NULL;
    }

    // return the ThreadWrapper (singleton)
    Trace::ReturnTrace();

    // PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::DoTest(const TestType testType)
{
    WebRtc_UWord32 ret(0);

    switch (testType)
    {
        case TTAll:
            ret = TestAudioLayerSelection();
            ret = TestDeviceEnumeration();
            ret = TestDeviceSelection();
            ret = TestAudioTransport();
            ret = TestSpeakerVolume();
            ret = TestMicrophoneVolume();
            ret = TestLoopback();
        case TTAudioLayerSelection:
            TestAudioLayerSelection();
            break;
        case TTDeviceEnumeration:
            ret = TestDeviceEnumeration();
            break;
        case TTDeviceSelection:
            ret = TestDeviceSelection();
            break;
        case TTAudioTransport:
            ret = TestAudioTransport();
            break;
        case TTSpeakerVolume:
            ret = TestSpeakerVolume();
            break;
        case TTMicrophoneVolume:
            ret = TestMicrophoneVolume();
            break;
        case TTSpeakerMute:
            ret = TestSpeakerMute();
            break;
        case TTMicrophoneMute:
            ret = TestMicrophoneMute();
            break;
        case TTMicrophoneBoost:
            ret = TestMicrophoneBoost();
            break;
        case TTMicrophoneAGC:
            ret = TestMicrophoneAGC();
            break;
        case TTLoopback:
            ret = TestLoopback();
            break;
        case TTDeviceRemoval:
            ret = TestDeviceRemoval();
            break;
        case TTMobileAPI:
            ret = TestAdvancedMBAPI();
        case TTTest:
            ret = TestExtra();
            break;
        default:
            break;
    }

    return 0;
}
;

WebRtc_Word32 FuncTestManager::TestAudioLayerSelection()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Audio Layer test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    AudioDeviceModule::AudioLayer audioLayer;
    TEST(audioDevice->ActiveAudioLayer(&audioLayer) == 0);

    if (audioLayer == AudioDeviceModule::kWindowsWaveAudio)
    {
        TEST_LOG("\nActiveAudioLayer: kWindowsWaveAudio\n \n");
    } else if (audioLayer == AudioDeviceModule::kWindowsCoreAudio)
    {
        TEST_LOG("\nActiveAudioLayer: kWindowsCoreAudio\n \n");
    } else if (audioLayer == AudioDeviceModule::kLinuxAlsaAudio)
    {
        TEST_LOG("\nActiveAudioLayer: kLinuxAlsaAudio\n \n");
    } else if (audioLayer == AudioDeviceModule::kLinuxPulseAudio)
    {
        TEST_LOG("\nActiveAudioLayer: kLinuxPulseAudio\n \n");
    } else
    {
        TEST_LOG("\nActiveAudioLayer: INVALID\n \n");
    }

    char ch;
    bool tryWinWave(false);
    bool tryWinCore(false);

    if (audioLayer == AudioDeviceModule::kWindowsWaveAudio)
    {
        TEST_LOG("Would you like to try kWindowsCoreAudio instead "
            "[requires Win Vista or Win 7] (Y/N)?\n: ");
        TEST(scanf(" %c", &ch) > 0);
        ch = toupper(ch);
        if (ch == 'Y')
        {
            tryWinCore = true;
        }
    } else if (audioLayer == AudioDeviceModule::kWindowsCoreAudio)
    {
        TEST_LOG("Would you like to try kWindowsWaveAudio instead (Y/N)?\n: ");
        TEST(scanf(" %c", &ch) > 0);
        ch = toupper(ch);
        if (ch == 'Y')
        {
            tryWinWave = true;
        }
    }

    if (tryWinWave || tryWinCore)
    {
        // =======================================
        // First, close down what we have started

        // terminate
        TEST(_audioDevice->RegisterEventObserver(NULL) == 0);
        TEST(_audioDevice->RegisterAudioCallback(NULL) == 0);
        TEST(_audioDevice->Terminate() == 0);

        // release the ProcessThread object
        if (_processThread)
        {
            _processThread->DeRegisterModule(_audioDevice);
            _processThread->Stop();
            ProcessThread::DestroyProcessThread(_processThread);
        }

        // delete the audio observer
        if (_audioEventObserver)
        {
            delete _audioEventObserver;
            _audioEventObserver = NULL;
        }

        // delete the audio transport
        if (_audioTransport)
        {
            delete _audioTransport;
            _audioTransport = NULL;
        }

        // release the AudioDeviceModule object
        if (_audioDevice)
        {
            TEST(_audioDevice->Release() == 0);
            _audioDevice = NULL;
        }

        // ==================================================
        // Next, try to make fresh start with new audio layer

        TEST((_processThread = ProcessThread::CreateProcessThread()) != NULL);
        if (_processThread == NULL)
        {
            return -1;
        }
        _processThread->Start();

        // create the Audio Device module based on selected audio layer
        if (tryWinWave)
        {
            _audioDevice = AudioDeviceModuleImpl::Create(
                555,
                AudioDeviceModule::kWindowsWaveAudio);
        } else if (tryWinCore)
        {
            _audioDevice = AudioDeviceModuleImpl::Create(
                555,
                AudioDeviceModule::kWindowsCoreAudio);
        }

        if (_audioDevice == NULL)
        {
            TEST_LOG("\nERROR: Switch of audio layer failed!\n");
            // restore default audio layer instead
            TEST((_audioDevice = AudioDeviceModuleImpl::Create(
                555, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
        }

        if (_audioDevice == NULL)
        {
            TEST_LOG("\nERROR: Failed to revert back to default audio layer!\n");
            return -1;
        }

        TEST(_audioDevice->AddRef() == 1);

        // register the Audio Device module
        _processThread->RegisterModule(_audioDevice);

        // register event observer
        _audioEventObserver = new AudioEventObserver(_audioDevice);
        TEST(_audioDevice->RegisterEventObserver(_audioEventObserver) == 0);

        // register audio transport
        _audioTransport = new AudioTransportImpl(_audioDevice);
        TEST(_audioDevice->RegisterAudioCallback(_audioTransport) == 0);

        TEST(_audioDevice->ActiveAudioLayer(&audioLayer) == 0);

        if (audioLayer == AudioDeviceModule::kWindowsWaveAudio)
        {
            if (tryWinCore)
                TEST_LOG("\nActiveAudioLayer: kWindowsWaveAudio <=> "
                    "switch was *not* possible\n \n");
            else
                TEST_LOG("\nActiveAudioLayer: kWindowsWaveAudio <=> "
                    "switch was possible\n \n");
        } else if (audioLayer == AudioDeviceModule::kWindowsCoreAudio)
        {
            if (tryWinWave)
                TEST_LOG("\nActiveAudioLayer: kWindowsCoreAudio <=> "
                    "switch was *not* possible\n \n");
            else
                TEST_LOG("\nActiveAudioLayer: kWindowsCoreAudio <=> "
                    "switch was possible\n \n");
        }
    } // if (tryWinWave || tryWinCore)

    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestDeviceEnumeration()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Device Enumeration test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    WebRtc_Word8 name[kAdmMaxDeviceNameSize];
    WebRtc_Word8 guid[kAdmMaxGuidSize];

    const WebRtc_Word16 nPlayoutDevices(audioDevice->PlayoutDevices());
    TEST(nPlayoutDevices >= 0);
    TEST_LOG("\nPlayoutDevices: %u\n \n", nPlayoutDevices);
    for (int n = 0; n < nPlayoutDevices; n++)
    {
        TEST(audioDevice->PlayoutDeviceName(n, name, guid) == 0);
        TEST_LOG(
                 "PlayoutDeviceName(%d) :   name=%s \n \
	                 guid=%s\n",
                 n, name, guid);
    }

#ifdef _WIN32
    // default (-1)
    TEST(audioDevice->PlayoutDeviceName(-1, name, guid) == 0);
    TEST_LOG("PlayoutDeviceName(%d):   default name=%s \n \
	                 default guid=%s\n", -1, name, guid);
#else
    // should fail
    TEST(audioDevice->PlayoutDeviceName(-1, name, guid) == -1);
#endif

    const WebRtc_Word16 nRecordingDevices(audioDevice->RecordingDevices());
    TEST(nRecordingDevices >= 0);
    TEST_LOG("\nRecordingDevices: %u\n \n", nRecordingDevices);
    for (int n = 0; n < nRecordingDevices; n++)
    {
        TEST(audioDevice->RecordingDeviceName(n, name, guid) == 0);
        TEST_LOG(
                 "RecordingDeviceName(%d) : name=%s \n \
	                 guid=%s\n",
                 n, name, guid);
    }

#ifdef _WIN32
    // default (-1)
    TEST(audioDevice->RecordingDeviceName(-1, name, guid) == 0);
    TEST_LOG("RecordingDeviceName(%d): default name=%s \n \
	                 default guid=%s\n", -1, name, guid);
#else
    // should fail
    TEST(audioDevice->PlayoutDeviceName(-1, name, guid) == -1);
#endif

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestDeviceSelection()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Device Selection test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

#define PRINT_HEADING(a, b) \
	{ \
		TEST_LOG("Set" #a "Device(" #b ") => \n"); \
	} \

#define PRINT_HEADING_IDX(a, b,c ) \
	{ \
		TEST_LOG("Set" #a "Device(%d) (%s) => \n", b, c); \
	} \

#define PRINT_STR(a, b) \
	{ \
                char str[128]; \
                (b == true) ? (sprintf(str, "  %-17s: available\n", #a)) : (sprintf(str, "  %-17s: NA\n", #a)); \
                TEST_LOG("%s", str); \
	} \

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    bool available(false);
    WebRtc_Word16 nDevices(-1);
    WebRtc_Word8 name[kAdmMaxDeviceNameSize];
    WebRtc_Word8 guid[kAdmMaxGuidSize];

    // =======
    // Playout

    nDevices = audioDevice->PlayoutDevices();
    TEST(nDevices >= 0);

    TEST_LOG("\n");
#ifdef _WIN32
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    PRINT_HEADING(Playout, kDefaultCommunicationDevice);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    PRINT_STR(Playout, available);
    if (available)
    {
        TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
        PRINT_STR(Stereo Playout, available);
    }
    else
    {
        PRINT_STR(Stereo Playout, false);
    }
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    PRINT_STR(Speaker, available);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    PRINT_STR(Speaker Volume, available);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    PRINT_STR(Speaker Mute, available);

    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == 0);
    PRINT_HEADING(Playout, kDefaultDevice);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    PRINT_STR(Playout, available);
    if (available)
    {
        TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
        PRINT_STR(Stereo Playout, available);
    }
    else
    {
        PRINT_STR(Stereo Playout, false);
    }
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    PRINT_STR(Speaker, available);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    PRINT_STR(Speaker Volume, available);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    PRINT_STR(Speaker Mute, available);
#else
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == -1);
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == -1);
#endif

    for (int i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->PlayoutDeviceName(i, name, guid) == 0);
        PRINT_HEADING_IDX(Playout, i, name);
        TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
        PRINT_STR(Playout, available);
        if (available)
        {
            TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
            PRINT_STR(Stereo Playout, available);
        } else
        {
            PRINT_STR(Stereo Playout, false);
        }
        TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
        PRINT_STR(Speaker, available);
        TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
        PRINT_STR(Speaker Volume, available);
        TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
        PRINT_STR(Speaker Mute, available);
    }

    // =========
    // Recording

    nDevices = audioDevice->RecordingDevices();
    TEST(nDevices >= 0);

    TEST_LOG("\n");
#ifdef _WIN32
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    PRINT_HEADING(Recording, kDefaultCommunicationDevice);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    PRINT_STR(Recording, available);
    if (available)
    {
        TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
        PRINT_STR(Stereo Recording, available);
    }
    else
    {
        // special fix to ensure that we don't log 'available' when recording is not OK
        PRINT_STR(Stereo Recording, false);
    }
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    PRINT_STR(Microphone, available);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    PRINT_STR(Microphone Volume, available);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    PRINT_STR(Microphone Mute, available);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    PRINT_STR(Microphone Boost, available);

    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice) == 0);
    PRINT_HEADING(Recording, kDefaultDevice);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    PRINT_STR(Recording, available);
    if (available)
    {
        TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
        PRINT_STR(Stereo Recording, available);
    }
    else
    {
        // special fix to ensure that we don't log 'available' when recording is not OK
        PRINT_STR(Stereo Recording, false);
    }
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    PRINT_STR(Microphone, available);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    PRINT_STR(Microphone Volume, available);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    PRINT_STR(Microphone Mute, available);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    PRINT_STR(Microphone Boost, available);
#else
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == -1);
    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice) == -1);
#endif

    for (int i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->RecordingDeviceName(i, name, guid) == 0);
        PRINT_HEADING_IDX(Recording, i, name);
        TEST(audioDevice->RecordingIsAvailable(&available) == 0);
        PRINT_STR(Recording, available);
        if (available)
        {
            TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
            PRINT_STR(Stereo Recording, available);
        } else
        {
            // special fix to ensure that we don't log 'available' when recording
            // is not OK
            PRINT_STR(Stereo Recording, false);
        }
        TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
        PRINT_STR(Microphone, available);
        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        PRINT_STR(Microphone Volume, available);
        TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
        PRINT_STR(Microphone Mute, available);
        TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
        PRINT_STR(Microphone Boost, available);
    }

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestAudioTransport()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Audio Transport test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    bool recIsAvailable(false);
    bool playIsAvailable(false);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->RecordingIsAvailable(&recIsAvailable) == 0);
    if (!recIsAvailable)
    {
        TEST_LOG(
                 "\nWARNING: Recording is not available for the selected device!\n \n");
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&playIsAvailable) == 0);
    if (recIsAvailable && playIsAvailable)
    {
        _audioTransport->SetFullDuplex(true);
    } else if (!playIsAvailable)
    {
        TEST_LOG(
                 "\nWARNING: Playout is not available for the selected device!\n \n");
    }

    bool available(false);
    WebRtc_UWord32 samplesPerSec(0);

    if (playIsAvailable)
    {
        // =========================================
        // Start by playing out an existing PCM file

        TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
        if (available)
        {
            WebRtc_UWord32 maxVolume(0);
            TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
            TEST(audioDevice->SetSpeakerVolume(maxVolume/2) == 0);
        }

        TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);

        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutSampleRate(&samplesPerSec) == 0);
        if (samplesPerSec == 48000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile48.c_str()));
        } else if (samplesPerSec == 44100 || samplesPerSec == 44000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile44.c_str()));
        } else if (samplesPerSec == 16000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile16.c_str()));
        } else if (samplesPerSec == 8000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile8.c_str()));
        } else {
            TEST_LOG("\nERROR: Sample rate (%u) is not supported!\n \n",
                     samplesPerSec);
            return -1;
        }
        TEST(audioDevice->StartPlayout() == 0);

        if (audioDevice->Playing())
        {
            TEST_LOG("\n> Listen to the file being played (fs=%d) out "
                "and verify that the audio quality is OK.\n"
                "> Press any key to stop playing...\n \n",
                samplesPerSec);
            PAUSE(DEFAULT_PAUSE_TIME);
        }

        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

        _audioTransport->SetFilePlayout(false);
    }

    bool enabled(false);
    if (recIsAvailable)
    {
        // ====================================
        // Next, record from microphone to file

        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        if (available)
        {
            WebRtc_UWord32 maxVolume(0);
            TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(audioDevice->SetMicrophoneVolume(maxVolume) == 0);
        }

        TEST(audioDevice->StartRawInputFileRecording(
            GetFilename(RecordedMicrophoneFile)) == 0);
        TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);

        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            // ensure file recording in mono
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft) == 0);
        }
        TEST(audioDevice->StartRecording() == 0);
        AudioDeviceUtility::Sleep(100);

        TEST(audioDevice->Recording() == true);
        if (audioDevice->Recording())
        {
            TEST_LOG("\n \n> The microphone input signal is now being recorded "
                "to a PCM file.\n"
                "> Speak into the microphone to ensure that your voice is"
                " recorded.\n> Press any key to stop recording...\n \n");
            PAUSE(DEFAULT_PAUSE_TIME);
        }

        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelBoth) == 0);
        }
        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
        TEST(audioDevice->StopRawInputFileRecording() == 0);
    }

    if (recIsAvailable && playIsAvailable)
    {
        // ==========================
        // Play out the recorded file

        _audioTransport->SetFilePlayout(true,
                                        GetFilename(RecordedMicrophoneFile));

        TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
        TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->InitPlayout() == 0);
            TEST(audioDevice->StartPlayout() == 0);
            AudioDeviceUtility::Sleep(100);
        }

        TEST(audioDevice->Playing() == true);
        if (audioDevice->Playing())
        {
            TEST_LOG("\n \n> Listen to the recorded file and verify that the "
                "audio quality is OK.\n"
                "> Press any key to stop listening...\n \n");
            PAUSE(DEFAULT_PAUSE_TIME);
        }

        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

        _audioTransport->SetFilePlayout(false);
    }

    if (recIsAvailable && playIsAvailable)
    {
        // ==============================
        // Finally, make full duplex test

        WebRtc_UWord32 playSamplesPerSec(0);
        WebRtc_UWord32 recSamplesPerSecRec(0);

        TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);

        _audioTransport->SetFullDuplex(true);

        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        if (available)
        {
            WebRtc_UWord32 maxVolume(0);
            TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(audioDevice->SetMicrophoneVolume(maxVolume) == 0);
        }

        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutSampleRate(&playSamplesPerSec) == 0);
        TEST(audioDevice->RecordingSampleRate(&recSamplesPerSecRec) == 0);
        if (playSamplesPerSec != recSamplesPerSecRec)
        {
            TEST_LOG("\nERROR: sample rates does not match (fs_play=%u, fs_rec=%u)",
                     playSamplesPerSec, recSamplesPerSecRec);
            TEST(audioDevice->StopRecording() == 0);
            TEST(audioDevice->StopPlayout() == 0);
            TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
            _audioTransport->SetFullDuplex(false);
            return -1;
        }

        TEST(audioDevice->StartRecording() == 0);
        TEST(audioDevice->StartPlayout() == 0);
        AudioDeviceUtility::Sleep(100);

        if (audioDevice->Playing() && audioDevice->Recording())
        {
            TEST_LOG("\n \n> Full duplex audio (fs=%u) is now active.\n"
                "> Speak into the microphone and verify that your voice is "
                "played out in loopback.\n> Press any key to stop...\n \n",
                     playSamplesPerSec);
            PAUSE(DEFAULT_PAUSE_TIME);
        }

        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

        _audioTransport->SetFullDuplex(false);
    }

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestSpeakerVolume()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Speaker Volume test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    WebRtc_UWord32 startVolume(0);
    WebRtc_UWord32 samplesPerSec(0);

    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetSpeakerVolume(true);
    } else
    {
        TEST_LOG("\nERROR: Volume control is not available for the selected "
            "device!\n \n");
        return -1;
    }

    // store initial volume setting
    TEST(audioDevice->InitSpeaker() == 0);
    TEST(audioDevice->SpeakerVolume(&startVolume) == 0);

    // start at volume 0
    TEST(audioDevice->SetSpeakerVolume(0) == 0);

    // ======================================
    // Start playing out an existing PCM file

    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutSampleRate(&samplesPerSec) == 0);
        if (48000 == samplesPerSec) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile48.c_str()));
        } else if (44100 == samplesPerSec || samplesPerSec == 44000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile44.c_str()));
        } else if (samplesPerSec == 16000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile16.c_str()));
        } else if (samplesPerSec == 8000) {
            _audioTransport->SetFilePlayout(
                true, GetResource(_playoutFile8.c_str()));
        } else {
            TEST_LOG("\nERROR: Sample rate (%d) is not supported!\n \n",
                     samplesPerSec);
            return -1;
        }
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->Playing() == true);
    if (audioDevice->Playing())
    {
        TEST_LOG("\n> Listen to the file being played out and verify that the "
            "selected speaker volume is varied between [~0] and [~MAX].\n"
            "> The file shall be played out with an increasing volume level "
            "correlated to the speaker volume.\n"
            "> Press any key to stop playing...\n \n");
        PAUSE(10000);
    }

    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    _audioTransport->SetSpeakerVolume(false);
    _audioTransport->SetFilePlayout(false);

    // restore volume setting
    TEST(audioDevice->SetSpeakerVolume(startVolume) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestSpeakerMute()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Speaker Mute test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    bool startMute(false);
    WebRtc_UWord32 samplesPerSec(0);

    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetSpeakerMute(true);
    } else
    {
        TEST_LOG(
                 "\nERROR: Mute control is not available for the selected"
                 " device!\n \n");
        return -1;
    }

    // store initial mute setting
    TEST(audioDevice->InitSpeaker() == 0);
    TEST(audioDevice->SpeakerMute(&startMute) == 0);

    // start with no mute
    TEST(audioDevice->SetSpeakerMute(false) == 0);

    // ======================================
    // Start playing out an existing PCM file

    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutSampleRate(&samplesPerSec) == 0);
        if (48000 == samplesPerSec)
            _audioTransport->SetFilePlayout(true, _playoutFile48.c_str());
        else if (44100 == samplesPerSec || 44000 == samplesPerSec)
            _audioTransport->SetFilePlayout(true, _playoutFile44.c_str());
        else
        {
            TEST_LOG("\nERROR: Sample rate (%d) is not supported!\n \n",
                     samplesPerSec);
            return -1;
        }
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->Playing() == true);
    if (audioDevice->Playing())
    {
        TEST_LOG("\n> Listen to the file being played out and verify that the"
            " selected speaker mute control is toggled between [MUTE ON] and"
            " [MUTE OFF].\n> You should only hear the file during the"
            " 'MUTE OFF' periods.\n"
            "> Press any key to stop playing...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    _audioTransport->SetSpeakerMute(false);
    _audioTransport->SetFilePlayout(false);

    // restore mute setting
    TEST(audioDevice->SetSpeakerMute(startMute) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestMicrophoneVolume()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Microphone Volume test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetMicrophoneVolume(true);
    } else
    {
        TEST_LOG("\nERROR: Volume control is not available for the selected "
            "device!\n \n");
        return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetFullDuplex(true);
    } else
    {
        TEST_LOG("\nERROR: Playout is not available for the selected "
            "device!\n \n");
        return -1;
    }

    TEST_LOG("\nEnable recording of microphone input to file (%s) during this"
        " test (Y/N)?\n: ",
             RecordedMicrophoneVolumeFile);
    char ch;
    bool fileRecording(false);
    TEST(scanf(" %c", &ch) > 0);
    ch = toupper(ch);
    if (ch == 'Y')
    {
        fileRecording = true;
    }

    WebRtc_UWord32 startVolume(0);
    bool enabled(false);

    // store initial volume setting
    TEST(audioDevice->InitMicrophone() == 0);
    TEST(audioDevice->MicrophoneVolume(&startVolume) == 0);

    // start at volume 0
    TEST(audioDevice->SetMicrophoneVolume(0) == 0);

    // ======================================================================
    // Start recording from the microphone while the mic volume is changed
    // continuously.
    // Also, start playing out the input to enable real-time verification.

    if (fileRecording)
    {
        TEST(audioDevice->StartRawInputFileRecording(RecordedMicrophoneVolumeFile) == 0);
    }
    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            // ensures a mono file
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelRight) == 0);
        }
        TEST(audioDevice->StartRecording() == 0);
    }
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->Recording() == true);
    TEST(audioDevice->Playing() == true);
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the selected "
            "microphone volume is varied between [~0] and [~MAX].\n"
            "> You should hear your own voice with an increasing volume level"
            " correlated to the microphone volume.\n"
            "> After a finalized test (and if file recording was enabled) "
            "verify the recorded result off line.\n"
            "> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    if (fileRecording)
    {
        TEST(audioDevice->StopRawInputFileRecording() == 0);
    }
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);

    _audioTransport->SetMicrophoneVolume(false);
    _audioTransport->SetFullDuplex(false);

    // restore volume setting
    TEST(audioDevice->SetMicrophoneVolume(startVolume) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestMicrophoneMute()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Microphone Mute test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetMicrophoneMute(true);
    } else
    {
        TEST_LOG("\nERROR: Mute control is not available for the selected"
            " device!\n \n");
        return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetFullDuplex(true);
    } else
    {
        TEST_LOG("\nERROR: Playout is not available for the selected "
            "device!\n \n");
        return -1;
    }

    TEST_LOG("\nEnable recording of microphone input to file (%s) during this "
        "test (Y/N)?\n: ",
        RecordedMicrophoneMuteFile);
    char ch;
    bool fileRecording(false);
    TEST(scanf(" %c", &ch) > 0);
    ch = toupper(ch);
    if (ch == 'Y')
    {
        fileRecording = true;
    }

    bool startMute(false);
    bool enabled(false);

    // store initial volume setting
    TEST(audioDevice->InitMicrophone() == 0);
    TEST(audioDevice->MicrophoneMute(&startMute) == 0);

    // start at no mute
    TEST(audioDevice->SetMicrophoneMute(false) == 0);

    // ==================================================================
    // Start recording from the microphone while the mic mute is toggled
    // continuously.
    // Also, start playing out the input to enable real-time verification.

    if (fileRecording)
    {
        TEST(audioDevice->StartRawInputFileRecording(RecordedMicrophoneMuteFile) == 0);
    }
    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            // ensure file recording in mono
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft) == 0);
        }
        TEST(audioDevice->StartRecording() == 0);
    }
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->Recording() == true);
    TEST(audioDevice->Playing() == true);
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the selected "
            "microphone mute control is toggled between [MUTE ON] and [MUTE OFF]."
            "\n> You should only hear your own voice in loopback during the"
            " 'MUTE OFF' periods.\n> After a finalized test (and if file "
            "recording was enabled) verify the recorded result off line.\n"
            "> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    if (fileRecording)
    {
        TEST(audioDevice->StopRawInputFileRecording() == 0);
    }
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    _audioTransport->SetMicrophoneMute(false);
    _audioTransport->SetFullDuplex(false);

    // restore volume setting
    TEST(audioDevice->SetMicrophoneMute(startMute) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestMicrophoneBoost()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Microphone Boost test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetMicrophoneBoost(true);
    } else
    {
        TEST_LOG(
                 "\nERROR: Boost control is not available for the selected device!\n \n");
        return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetFullDuplex(true);
    } else
    {
        TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
        return -1;
    }

    TEST_LOG("\nEnable recording of microphone input to file (%s) during this "
        "test (Y/N)?\n: ",
        RecordedMicrophoneBoostFile);
    char ch;
    bool fileRecording(false);
    TEST(scanf(" %c", &ch) > 0);
    ch = toupper(ch);
    if (ch == 'Y')
    {
        fileRecording = true;
    }

    bool startBoost(false);
    bool enabled(false);

    // store initial volume setting
    TEST(audioDevice->InitMicrophone() == 0);
    TEST(audioDevice->MicrophoneBoost(&startBoost) == 0);

    // start at no boost
    TEST(audioDevice->SetMicrophoneBoost(false) == 0);

    // ==================================================================
    // Start recording from the microphone while the mic boost is toggled
    // continuously.
    // Also, start playing out the input to enable real-time verification.

    if (fileRecording)
    {
        TEST(audioDevice->StartRawInputFileRecording(RecordedMicrophoneBoostFile) == 0);
    }
    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            // ensure file recording in mono
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft) == 0);
        }
        TEST(audioDevice->StartRecording() == 0);
    }
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->Recording() == true);
    TEST(audioDevice->Playing() == true);
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the selected "
            "microphone boost control is toggled between [BOOST ON] and [BOOST OFF].\n"
            "> You should hear your own voice with an increased volume level "
            "during the 'BOOST ON' periods.\n \n"
            "> After a finalized test (and if file recording was enabled) verify"
            " the recorded result off line.\n"
        "> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    if (fileRecording)
    {
        TEST(audioDevice->StopRawInputFileRecording() == 0);
    }
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    _audioTransport->SetMicrophoneBoost(false);
    _audioTransport->SetFullDuplex(false);

    // restore boost setting
    TEST(audioDevice->SetMicrophoneBoost(startBoost) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestMicrophoneAGC()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Microphone AGC test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    bool available(false);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetMicrophoneAGC(true);
    } else
    {
        TEST_LOG("\nERROR: It is not possible to control the microphone volume"
            " for the selected device!\n \n");
        return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        _audioTransport->SetFullDuplex(true);
    } else
    {
        TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
        return -1;
    }

    TEST_LOG("\nEnable recording of microphone input to file (%s) during "
        "this test (Y/N)?\n: ",
        RecordedMicrophoneAGCFile);
    char ch;
    bool fileRecording(false);
    TEST(scanf(" %c", &ch) > 0);
    ch = toupper(ch);
    if (ch == 'Y')
    {
        fileRecording = true;
    }

    WebRtc_UWord32 startVolume(0);
    bool enabled(false);

    // store initial volume setting
    TEST(audioDevice->InitMicrophone() == 0);
    TEST(audioDevice->MicrophoneVolume(&startVolume) == 0);

    // ====================================================================
    // Start recording from the microphone while the mic volume is changed
    // continuously
    // by the emulated AGC (implemented by our audio transport).
    // Also, start playing out the input to enable real-time verification.

    if (fileRecording)
    {
        TEST(audioDevice->StartRawInputFileRecording(RecordedMicrophoneAGCFile) == 0);
    }
    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetAGC(true) == 0);
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        if (enabled)
        {
            // ensures a mono file
            TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelRight) == 0);
        }
        TEST(audioDevice->StartRecording() == 0);
    }
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
    }

    TEST(audioDevice->AGC() == true);
    TEST(audioDevice->Recording() == true);
    TEST(audioDevice->Playing() == true);
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the volume of"
            " the selected microphone is varied between [~0] and [~MAX].\n"
            "> You should hear your own voice with an increasing volume level"
            " correlated to an emulated AGC setting.\n"
            "> After a finalized test (and if file recording was enabled) verify"
            " the recorded result off line.\n"
            "> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    if (fileRecording)
    {
        TEST(audioDevice->StopRawInputFileRecording() == 0);
    }
    TEST(audioDevice->SetAGC(false) == 0);
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);

    _audioTransport->SetMicrophoneAGC(false);
    _audioTransport->SetFullDuplex(false);

    // restore volume setting
    TEST(audioDevice->SetMicrophoneVolume(startVolume) == 0);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestLoopback()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Loopback measurement test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    bool recIsAvailable(false);
    bool playIsAvailable(false);
    WebRtc_UWord8 nPlayChannels(0);
    WebRtc_UWord8 nRecChannels(0);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->RecordingIsAvailable(&recIsAvailable) == 0);
    if (!recIsAvailable)
    {
        TEST_LOG("\nERROR: Recording is not available for the selected device!\n \n");
        return -1;
    }

    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }

    TEST(audioDevice->PlayoutIsAvailable(&playIsAvailable) == 0);
    if (recIsAvailable && playIsAvailable)
    {
        _audioTransport->SetFullDuplex(true);
        _audioTransport->SetLoopbackMeasurements(true);
    } else if (!playIsAvailable)
    {
        TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
        return -1;
    }

    bool enabled(false);
    bool available(false);

    if (recIsAvailable && playIsAvailable)
    {
        WebRtc_UWord32 playSamplesPerSec(0);
        WebRtc_UWord32 recSamplesPerSecRec(0);

        TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);

        _audioTransport->SetFullDuplex(true);

        TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->SetStereoRecording(true) == 0);
        }

        TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->SetStereoPlayout(true) == 0);
        }

        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        if (available)
        {
            WebRtc_UWord32 maxVolume(0);
            TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(audioDevice->SetMicrophoneVolume(maxVolume) == 0);
        }

        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutSampleRate(&playSamplesPerSec) == 0);
        TEST(audioDevice->RecordingSampleRate(&recSamplesPerSecRec) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        enabled ? nPlayChannels = 2 : nPlayChannels = 1;
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        enabled ? nRecChannels = 2 : nRecChannels = 1;
        TEST(audioDevice->StartRecording() == 0);
        TEST(audioDevice->StartPlayout() == 0);

        if (audioDevice->Playing() && audioDevice->Recording())
        {
            TEST_LOG("\n \n> Loopback audio is now active.\n"
               "> Rec : fs=%u, #channels=%u.\n"
                "> Play: fs=%u, #channels=%u.\n"
                "> Speak into the microphone and verify that your voice is"
                "  played out in loopback.\n"
                "> Press any key to stop...\n \n",
                recSamplesPerSecRec, nRecChannels, playSamplesPerSec,
                nPlayChannels);
            PAUSE(30000);
        }

        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

        _audioTransport->SetFullDuplex(false);
        _audioTransport->SetLoopbackMeasurements(false);
    }

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestDeviceRemoval()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Device removal test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    bool recIsAvailable(false);
    bool playIsAvailable(false);
    WebRtc_UWord8 nPlayChannels(0);
    WebRtc_UWord8 nRecChannels(0);
    WebRtc_UWord8 loopCount(0);

    while (loopCount < 2)
    {
        if (SelectRecordingDevice() == -1)
        {
            TEST_LOG("\nERROR: Device selection failed!\n \n");
            return -1;
        }

        TEST(audioDevice->RecordingIsAvailable(&recIsAvailable) == 0);
        if (!recIsAvailable)
        {
            TEST_LOG("\nERROR: Recording is not available for the selected device!\n \n");
            return -1;
        }

        if (SelectPlayoutDevice() == -1)
        {
            TEST_LOG("\nERROR: Device selection failed!\n \n");
            return -1;
        }

        TEST(audioDevice->PlayoutIsAvailable(&playIsAvailable) == 0);
        if (recIsAvailable && playIsAvailable)
        {
            _audioTransport->SetFullDuplex(true);
        } else if (!playIsAvailable)
        {
            TEST_LOG("\nERROR: Playout is not available for the selected device!\n \n");
            return -1;
        }

        bool available(false);
        bool enabled(false);

        if (recIsAvailable && playIsAvailable)
        {
            WebRtc_UWord32 playSamplesPerSec(0);
            WebRtc_UWord32 recSamplesPerSecRec(0);

            TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);

            _audioTransport->SetFullDuplex(true);

            TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
            if (available)
            {
                TEST(audioDevice->SetStereoRecording(true) == 0);
            }

            TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
            if (available)
            {
                TEST(audioDevice->SetStereoPlayout(true) == 0);
            }

            TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
            if (available)
            {
                WebRtc_UWord32 maxVolume(0);
                TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
                TEST(audioDevice->SetMicrophoneVolume(maxVolume) == 0);
            }

            TEST(audioDevice->InitRecording() == 0);
            TEST(audioDevice->InitPlayout() == 0);
            TEST(audioDevice->PlayoutSampleRate(&playSamplesPerSec) == 0);
            TEST(audioDevice->RecordingSampleRate(&recSamplesPerSecRec) == 0);
            TEST(audioDevice->StereoPlayout(&enabled) == 0);
            enabled ? nPlayChannels = 2 : nPlayChannels = 1;
            TEST(audioDevice->StereoRecording(&enabled) == 0);
            enabled ? nRecChannels = 2 : nRecChannels = 1;
            TEST(audioDevice->StartRecording() == 0);
            TEST(audioDevice->StartPlayout() == 0);

            AudioDeviceModule::AudioLayer audioLayer;
            TEST(audioDevice->ActiveAudioLayer(&audioLayer) == 0);

            if (audioLayer == AudioDeviceModule::kLinuxPulseAudio)
            {
                TEST_LOG("\n \n> PulseAudio loopback audio is now active.\n"
                    "> Rec : fs=%u, #channels=%u.\n"
                    "> Play: fs=%u, #channels=%u.\n"
                    "> Speak into the microphone and verify that your voice is"
                    " played out in loopback.\n"
                    "> Unplug the device and make sure that your voice is played"
                    " out in loop back on the built-in soundcard.\n"
                    "> Then press any key...\n",
                         recSamplesPerSecRec, nRecChannels, playSamplesPerSec,
                         nPlayChannels);

                PAUSE(DEFAULT_PAUSE_TIME);
            } else if (audioDevice->Playing() && audioDevice->Recording())
            {
                if (loopCount < 1)
                {
                    TEST_LOG("\n \n> Loopback audio is now active.\n"
                        "> Rec : fs=%u, #channels=%u.\n"
                        "> Play: fs=%u, #channels=%u.\n"
                        "> Speak into the microphone and verify that your voice"
                        " is played out in loopback.\n"
                        "> Unplug the device and wait for the error message...\n",
                        recSamplesPerSecRec, nRecChannels,
                        playSamplesPerSec, nPlayChannels);

                    _audioEventObserver->_error
                        = (AudioDeviceObserver::ErrorCode) (-1);
                    while (_audioEventObserver->_error
                        == (AudioDeviceObserver::ErrorCode) (-1))
                    {
                        SLEEP(500);
                    }
                } else
                {
                    TEST_LOG("\n \n> Loopback audio is now active.\n"
                        "> Rec : fs=%u, #channels=%u.\n"
                        "> Play: fs=%u, #channels=%u.\n"
                        "> Speak into the microphone and verify that your voice"
                        " is played out in loopback.\n"
                        "> Press any key to stop...\n",
                             recSamplesPerSecRec, nRecChannels,
                             playSamplesPerSec, nPlayChannels);

                    PAUSE(DEFAULT_PAUSE_TIME);
                }
            }

            TEST(audioDevice->StopRecording() == 0);
            TEST(audioDevice->StopPlayout() == 0);
            TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

            _audioTransport->SetFullDuplex(false);

            if (loopCount < 1)
            {
                TEST_LOG("\n \n> Stopped!\n");
                TEST_LOG("> Now reinsert device if you want to enumerate it.\n");
                TEST_LOG("> Press any key when done.\n");
                PAUSE(DEFAULT_PAUSE_TIME);
            }

            loopCount++;
        }
    } // loopCount

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::TestExtra()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Extra test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

WebRtc_Word32 FuncTestManager::SelectRecordingDevice()
{
    WebRtc_Word16 nDevices = _audioDevice->RecordingDevices();
    WebRtc_Word8 name[kAdmMaxDeviceNameSize];
    WebRtc_Word8 guid[kAdmMaxGuidSize];
    WebRtc_Word32 ret(-1);

#ifdef _WIN32
    TEST_LOG("\nSelect Recording Device\n \n");
    TEST_LOG("  (%d) Default\n", 0);
    TEST_LOG("  (%d) Default Communication [Win 7]\n", 1);
    TEST_LOG("- - - - - - - - - - - - - - - - - - - -\n");
    for (int i = 0; i < nDevices; i++)
    {
        TEST(_audioDevice->RecordingDeviceName(i, name, guid) == 0);
        TEST_LOG(" (%d) Device %d (%s)\n", i+10, i, name);
    }
    TEST_LOG("\n: ");

    int sel(0);

    scanf("%u", &sel);

    if (sel == 0)
    {
        TEST((ret = _audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice)) == 0);
    }
    else if (sel == 1)
    {
        TEST((ret = _audioDevice->SetRecordingDevice(
            AudioDeviceModule::kDefaultCommunicationDevice)) == 0);
    }
    else if (sel < (nDevices+10))
    {
        TEST((ret = _audioDevice->SetRecordingDevice(sel-10)) == 0);
    }
    else
    {
        return -1;
    }
#else
    TEST_LOG("\nSelect Recording Device\n \n");
    for (int i = 0; i < nDevices; i++)
    {
        TEST(_audioDevice->RecordingDeviceName(i, name, guid) == 0);
        TEST_LOG(" (%d) Device %d (%s)\n", i, i, name);
    }
    TEST_LOG("\n: ");
    int sel(0);
    TEST(scanf("%u", &sel) > 0);
    if (sel < (nDevices))
    {
        TEST((ret = _audioDevice->SetRecordingDevice(sel)) == 0);
    } else
    {
        return -1;
    }
#endif

    return ret;
}

WebRtc_Word32 FuncTestManager::SelectPlayoutDevice()
{
    WebRtc_Word16 nDevices = _audioDevice->PlayoutDevices();
    WebRtc_Word8 name[kAdmMaxDeviceNameSize];
    WebRtc_Word8 guid[kAdmMaxGuidSize];

#ifdef _WIN32
    TEST_LOG("\nSelect Playout Device\n \n");
    TEST_LOG("  (%d) Default\n", 0);
    TEST_LOG("  (%d) Default Communication [Win 7]\n", 1);
    TEST_LOG("- - - - - - - - - - - - - - - - - - - -\n");
    for (int i = 0; i < nDevices; i++)
    {
        TEST(_audioDevice->PlayoutDeviceName(i, name, guid) == 0);
        TEST_LOG(" (%d) Device %d (%s)\n", i+10, i, name);
    }
    TEST_LOG("\n: ");

    int sel(0);

    scanf("%u", &sel);

    WebRtc_Word32 ret(0);

    if (sel == 0)
    {
        TEST((ret = _audioDevice->SetPlayoutDevice(
            AudioDeviceModule::kDefaultDevice)) == 0);
    }
    else if (sel == 1)
    {
        TEST((ret = _audioDevice->SetPlayoutDevice(
            AudioDeviceModule::kDefaultCommunicationDevice)) == 0);
    }
    else if (sel < (nDevices+10))
    {
        TEST((ret = _audioDevice->SetPlayoutDevice(sel-10)) == 0);
    }
    else
    {
        return -1;
    }
#else
    TEST_LOG("\nSelect Playout Device\n \n");
    for (int i = 0; i < nDevices; i++)
    {
        TEST(_audioDevice->PlayoutDeviceName(i, name, guid) == 0);
        TEST_LOG(" (%d) Device %d (%s)\n", i, i, name);
    }
    TEST_LOG("\n: ");
    int sel(0);
    TEST(scanf("%u", &sel) > 0);
    WebRtc_Word32 ret(0);
    if (sel < (nDevices))
    {
        TEST((ret = _audioDevice->SetPlayoutDevice(sel)) == 0);
    } else
    {
        return -1;
    }
#endif

    return ret;
}

WebRtc_Word32 FuncTestManager::TestAdvancedMBAPI()
{
    TEST_LOG("\n=======================================\n");
    TEST_LOG(" Advanced mobile device API test:\n");
    TEST_LOG("=======================================\n");

    if (_audioDevice == NULL)
    {
        return -1;
    }

    RESET_TEST;

    AudioDeviceModule* audioDevice = _audioDevice;

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);

    if (SelectRecordingDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }
    if (SelectPlayoutDevice() == -1)
    {
        TEST_LOG("\nERROR: Device selection failed!\n \n");
        return -1;
    }
    _audioTransport->SetFullDuplex(true);
    _audioTransport->SetLoopbackMeasurements(true);

    TEST(audioDevice->RegisterAudioCallback(_audioTransport) == 0);
    // Start recording
    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->StartRecording() == 0);
    // Start playout
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);

    TEST(audioDevice->Recording() == true);
    TEST(audioDevice->Playing() == true);

#if defined(_WIN32_WCE) || defined(MAC_IPHONE)
    TEST_LOG("\nResetAudioDevice\n \n");
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the audio is good.\n\
> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }
    for (int p=0; p<=60; p+=20)
    {
        TEST_LOG("Resetting sound device several time with pause %d ms\n", p);
        for (int l=0; l<20; ++l)
        {
            TEST(audioDevice->ResetAudioDevice() == 0);
            AudioDeviceUtility::Sleep(p);
        }
        TEST_LOG("\n> Speak into the microphone and verify that the audio is good.\n");
        AudioDeviceUtility::Sleep(2000);
    }
#endif

#if defined(MAC_IPHONE)
    bool loudspeakerOn(false);
    TEST_LOG("\nSet playout spaker\n \n");
    if (audioDevice->Recording() && audioDevice->Playing())
    {
        TEST_LOG("\n> Speak into the microphone and verify that the audio is good.\n\
> Press any key to stop...\n \n");
        PAUSE(DEFAULT_PAUSE_TIME);
    }

    TEST_LOG("Set to use speaker\n");
    TEST(audioDevice->SetLoudspeakerStatus(true) == 0);
    TEST_LOG("\n> Speak into the microphone and verify that the audio is"
        " from the loudspeaker.\n\
> Press any key to stop...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == true);

    TEST_LOG("Set to not use speaker\n");
    TEST(audioDevice->SetLoudspeakerStatus(false) == 0);
    TEST_LOG("\n> Speak into the microphone and verify that the audio is not"
        " from the loudspeaker.\n\
> Press any key to stop...\n \n");
    PAUSE(DEFAULT_PAUSE_TIME);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == false);
#endif

    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    _audioTransport->SetFullDuplex(false);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}

} // namespace webrtc

// EOF
