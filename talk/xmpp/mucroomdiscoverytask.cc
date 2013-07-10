/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/xmpp/mucroomdiscoverytask.h"

#include "talk/xmpp/constants.h"

namespace buzz {

MucRoomDiscoveryTask::MucRoomDiscoveryTask(
    XmppTaskParentInterface* parent,
    const Jid& room_jid)
    : IqTask(parent, STR_GET, room_jid,
             new buzz::XmlElement(buzz::QN_DISCO_INFO_QUERY)) {
}

void MucRoomDiscoveryTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query = stanza->FirstNamed(QN_DISCO_INFO_QUERY);
  if (query == NULL) {
    SignalError(this, NULL);
    return;
  }

  std::set<std::string> features;
  std::map<std::string, std::string> extended_info;
  const XmlElement* identity = query->FirstNamed(QN_DISCO_IDENTITY);
  if (identity == NULL || !identity->HasAttr(QN_NAME)) {
    SignalResult(this, false, "", features, extended_info);
    return;
  }

  const std::string name(identity->Attr(QN_NAME));

  for (const XmlElement* feature = query->FirstNamed(QN_DISCO_FEATURE);
       feature != NULL; feature = feature->NextNamed(QN_DISCO_FEATURE)) {
    features.insert(feature->Attr(QN_VAR));
  }

  const XmlElement* data_x = query->FirstNamed(QN_XDATA_X);
  if (data_x != NULL) {
    for (const XmlElement* field = data_x->FirstNamed(QN_XDATA_FIELD);
         field != NULL; field = field->NextNamed(QN_XDATA_FIELD)) {
      const std::string key(field->Attr(QN_VAR));
      extended_info[key] = field->Attr(QN_XDATA_VALUE);
    }
  }

  SignalResult(this, true, name, features, extended_info);
}

}  // namespace buzz
