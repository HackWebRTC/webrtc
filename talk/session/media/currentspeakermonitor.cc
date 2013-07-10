/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#include "talk/session/media/currentspeakermonitor.h"

#include "talk/base/logging.h"
#include "talk/session/media/call.h"

namespace cricket {

namespace {
const int kMaxAudioLevel = 9;
// To avoid overswitching, we disable switching for a period of time after a
// switch is done.
const int kDefaultMinTimeBetweenSwitches = 1000;
}

CurrentSpeakerMonitor::CurrentSpeakerMonitor(Call* call, BaseSession* session)
    : started_(false),
      call_(call),
      session_(session),
      current_speaker_ssrc_(0),
      earliest_permitted_switch_time_(0),
      min_time_between_switches_(kDefaultMinTimeBetweenSwitches) {
}

CurrentSpeakerMonitor::~CurrentSpeakerMonitor() {
  Stop();
}

void CurrentSpeakerMonitor::Start() {
  if (!started_) {
    call_->SignalAudioMonitor.connect(
        this, &CurrentSpeakerMonitor::OnAudioMonitor);
    call_->SignalMediaStreamsUpdate.connect(
        this, &CurrentSpeakerMonitor::OnMediaStreamsUpdate);

    started_ = true;
  }
}

void CurrentSpeakerMonitor::Stop() {
  if (started_) {
    call_->SignalAudioMonitor.disconnect(this);
    call_->SignalMediaStreamsUpdate.disconnect(this);

    started_ = false;
    ssrc_to_speaking_state_map_.clear();
    current_speaker_ssrc_ = 0;
    earliest_permitted_switch_time_ = 0;
  }
}

void CurrentSpeakerMonitor::set_min_time_between_switches(
    uint32 min_time_between_switches) {
  min_time_between_switches_ = min_time_between_switches;
}

void CurrentSpeakerMonitor::OnAudioMonitor(Call* call, const AudioInfo& info) {
  std::map<uint32, int> active_ssrc_to_level_map;
  cricket::AudioInfo::StreamList::const_iterator stream_list_it;
  for (stream_list_it = info.active_streams.begin();
       stream_list_it != info.active_streams.end(); ++stream_list_it) {
    uint32 ssrc = stream_list_it->first;
    active_ssrc_to_level_map[ssrc] = stream_list_it->second;

    // It's possible we haven't yet added this source to our map.  If so,
    // add it now with a "not speaking" state.
    if (ssrc_to_speaking_state_map_.find(ssrc) ==
        ssrc_to_speaking_state_map_.end()) {
      ssrc_to_speaking_state_map_[ssrc] = SS_NOT_SPEAKING;
    }
  }

  int max_level = 0;
  uint32 loudest_speaker_ssrc = 0;

  // Update the speaking states of all participants based on the new audio
  // level information.  Also retain loudest speaker.
  std::map<uint32, SpeakingState>::iterator state_it;
  for (state_it = ssrc_to_speaking_state_map_.begin();
       state_it != ssrc_to_speaking_state_map_.end(); ++state_it) {
    bool is_previous_speaker = current_speaker_ssrc_ == state_it->first;

    // This uses a state machine in order to gradually identify
    // members as having started or stopped speaking. Matches the
    // algorithm used by the hangouts js code.

    std::map<uint32, int>::const_iterator level_it =
        active_ssrc_to_level_map.find(state_it->first);
    // Note that the stream map only contains streams with non-zero audio
    // levels.
    int level = (level_it != active_ssrc_to_level_map.end()) ?
        level_it->second : 0;
    switch (state_it->second) {
      case SS_NOT_SPEAKING:
        if (level > 0) {
          // Reset level because we don't think they're really speaking.
          level = 0;
          state_it->second = SS_MIGHT_BE_SPEAKING;
        } else {
          // State unchanged.
        }
        break;
      case SS_MIGHT_BE_SPEAKING:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_NOT_SPEAKING;
        }
        break;
      case SS_SPEAKING:
        if (level > 0) {
          // State unchanged.
        } else {
          state_it->second = SS_WAS_SPEAKING_RECENTLY1;
          if (is_previous_speaker) {
            // Assume this is an inter-word silence and assign him the highest
            // volume.
            level = kMaxAudioLevel;
          }
        }
        break;
      case SS_WAS_SPEAKING_RECENTLY1:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_WAS_SPEAKING_RECENTLY2;
          if (is_previous_speaker) {
            // Assume this is an inter-word silence and assign him the highest
            // volume.
            level = kMaxAudioLevel;
          }
        }
        break;
      case SS_WAS_SPEAKING_RECENTLY2:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_NOT_SPEAKING;
        }
        break;
    }

    if (level > max_level) {
      loudest_speaker_ssrc = state_it->first;
      max_level = level;
    } else if (level > 0 && level == max_level && is_previous_speaker) {
      // Favor continuity of loudest speakers if audio levels are equal.
      loudest_speaker_ssrc = state_it->first;
    }
  }

  // We avoid over-switching by disabling switching for a period of time after
  // a switch is done.
  uint32 now = talk_base::Time();
  if (earliest_permitted_switch_time_ <= now &&
      current_speaker_ssrc_ != loudest_speaker_ssrc) {
    current_speaker_ssrc_ = loudest_speaker_ssrc;
    LOG(LS_INFO) << "Current speaker changed to " << current_speaker_ssrc_;
    earliest_permitted_switch_time_ = now + min_time_between_switches_;
    SignalUpdate(this, current_speaker_ssrc_);
  }
}

void CurrentSpeakerMonitor::OnMediaStreamsUpdate(Call* call,
                                                 Session* session,
                                                 const MediaStreams& added,
                                                 const MediaStreams& removed) {
  if (call == call_ && session == session_) {
    // Update the speaking state map based on added and removed streams.
    for (std::vector<cricket::StreamParams>::const_iterator
           it = removed.video().begin(); it != removed.video().end(); ++it) {
      ssrc_to_speaking_state_map_.erase(it->first_ssrc());
    }

    for (std::vector<cricket::StreamParams>::const_iterator
           it = added.video().begin(); it != added.video().end(); ++it) {
      ssrc_to_speaking_state_map_[it->first_ssrc()] = SS_NOT_SPEAKING;
    }
  }
}

}  // namespace cricket
