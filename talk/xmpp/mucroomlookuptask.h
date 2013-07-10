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

#ifndef TALK_XMPP_MUCROOMLOOKUPTASK_H_
#define TALK_XMPP_MUCROOMLOOKUPTASK_H_

#include <string>
#include "talk/xmpp/iqtask.h"

namespace buzz {

struct MucRoomInfo {
  Jid jid;
  std::string name;
  std::string domain;
  std::string hangout_id;

  std::string full_name() const {
    return name + "@" + domain;
  }
};

class MucRoomLookupTask : public IqTask {
 public:
  static MucRoomLookupTask*
      CreateLookupTaskForRoomName(XmppTaskParentInterface* parent,
                                  const Jid& lookup_server_jid,
                                  const std::string& room_name,
                                  const std::string& room_domain);
  static MucRoomLookupTask*
      CreateLookupTaskForRoomJid(XmppTaskParentInterface* parent,
                                 const Jid& lookup_server_jid,
                                 const Jid& room_jid);
  static MucRoomLookupTask*
      CreateLookupTaskForHangoutId(XmppTaskParentInterface* parent,
                                   const Jid& lookup_server_jid,
                                   const std::string& hangout_id);
  static MucRoomLookupTask*
      CreateLookupTaskForExternalId(XmppTaskParentInterface* parent,
                                    const Jid& lookup_server_jid,
                                    const std::string& external_id,
                                    const std::string& type);

  sigslot::signal2<MucRoomLookupTask*,
                   const MucRoomInfo&> SignalResult;

 protected:
  virtual void HandleResult(const XmlElement* element);

 private:
  MucRoomLookupTask(XmppTaskParentInterface* parent,
                    const Jid& lookup_server_jid,
                    XmlElement* query);
  static XmlElement* MakeNameQuery(const std::string& room_name,
                                   const std::string& room_domain);
  static XmlElement* MakeJidQuery(const Jid& room_jid);
  static XmlElement* MakeHangoutIdQuery(const std::string& hangout_id);
  static XmlElement* MakeExternalIdQuery(const std::string& external_id,
                                         const std::string& type);
};

}  // namespace buzz

#endif  // TALK_XMPP_MUCROOMLOOKUPTASK_H_
