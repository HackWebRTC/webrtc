/*
 * libjingle
 * Copyright 2012 Google Inc, and Robin Seggelmann
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

#ifndef TALK_MEDIA_SCTP_HYBRIDDATAENGINE_H_
#define TALK_MEDIA_SCTP_HYBRIDDATAENGINE_H_

#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"

namespace cricket {

class HybridDataEngine : public DataEngineInterface {
 public:
  // Takes ownership.
  HybridDataEngine(DataEngineInterface* first,
                   DataEngineInterface* second)
      : first_(first),
        second_(second) {
    codecs_ = first_->data_codecs();
    codecs_.insert(
        codecs_.end(),
        second_->data_codecs().begin(),
        second_->data_codecs().end());
  }

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type) {
    DataMediaChannel* channel = NULL;
    if (first_) {
      channel = first_->CreateChannel(data_channel_type);
    }
    if (!channel && second_) {
      channel = second_->CreateChannel(data_channel_type);
    }
    return channel;
  }

  virtual const std::vector<DataCodec>& data_codecs() { return codecs_; }

 private:
  talk_base::scoped_ptr<DataEngineInterface> first_;
  talk_base::scoped_ptr<DataEngineInterface> second_;
  std::vector<DataCodec> codecs_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_SCTP_HYBRIDDATAENGINE_H_
