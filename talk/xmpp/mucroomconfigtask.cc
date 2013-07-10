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

#include <string>
#include <vector>

#include "talk/xmpp/mucroomconfigtask.h"

#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/constants.h"

namespace buzz {

MucRoomConfigTask::MucRoomConfigTask(
    XmppTaskParentInterface* parent,
    const Jid& room_jid,
    const std::string& room_name,
    const std::vector<std::string>& room_features)
    : IqTask(parent, STR_SET, room_jid,
             MakeRequest(room_name, room_features)),
      room_jid_(room_jid) {
}

XmlElement* MucRoomConfigTask::MakeRequest(
    const std::string& room_name,
    const std::vector<std::string>& room_features) {
  buzz::XmlElement* owner_query = new
      buzz::XmlElement(buzz::QN_MUC_OWNER_QUERY, true);

  buzz::XmlElement* x_form = new buzz::XmlElement(buzz::QN_XDATA_X, true);
  x_form->SetAttr(buzz::QN_TYPE, buzz::STR_FORM);

  buzz::XmlElement* roomname_field =
      new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
  roomname_field->SetAttr(buzz::QN_VAR, buzz::STR_MUC_ROOMCONFIG_ROOMNAME);
  roomname_field->SetAttr(buzz::QN_TYPE, buzz::STR_TEXT_SINGLE);

  buzz::XmlElement* roomname_value =
      new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
  roomname_value->SetBodyText(room_name);

  roomname_field->AddElement(roomname_value);
  x_form->AddElement(roomname_field);

  buzz::XmlElement* features_field =
      new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
  features_field->SetAttr(buzz::QN_VAR, buzz::STR_MUC_ROOMCONFIG_FEATURES);
  features_field->SetAttr(buzz::QN_TYPE, buzz::STR_LIST_MULTI);

  for (std::vector<std::string>::const_iterator feature = room_features.begin();
       feature != room_features.end(); ++feature) {
    buzz::XmlElement* features_value =
        new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
    features_value->SetBodyText(*feature);
    features_field->AddElement(features_value);
  }

  x_form->AddElement(features_field);
  owner_query->AddElement(x_form);
  return owner_query;
}

void MucRoomConfigTask::HandleResult(const XmlElement* element) {
  SignalResult(this);
}

}  // namespace buzz
