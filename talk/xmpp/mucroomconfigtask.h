/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_XMPP_MUCROOMCONFIGTASK_H_
#define TALK_XMPP_MUCROOMCONFIGTASK_H_

#include <string>
#include "talk/xmpp/iqtask.h"

namespace buzz {

// This task configures the muc room for document sharing and other enterprise
// specific goodies.
class MucRoomConfigTask : public IqTask {
 public:
  MucRoomConfigTask(XmppTaskParentInterface* parent,
                    const Jid& room_jid,
                    const std::string& room_name,
                    const std::vector<std::string>& room_features);

  // Room configuration does not return any reasonable error
  // values. The First config request configures the room, subseqent
  // ones are just ignored by server and server returns empty
  // response.
  sigslot::signal1<MucRoomConfigTask*> SignalResult;

  const Jid& room_jid() const { return room_jid_; }

 protected:
  virtual void HandleResult(const XmlElement* stanza);

 private:
  static XmlElement* MakeRequest(const std::string& room_name,
                                 const std::vector<std::string>& room_features);
  Jid room_jid_;
};

}  // namespace buzz

#endif  // TALK_XMPP_MUCROOMCONFIGTASK_H_
