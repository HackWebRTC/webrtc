/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

// Fires a disco items query, such as the following example:
//
//      <iq type='get'
//          from='foo@gmail.com/asdf'
//          to='bar@google.com'
//          id='1234'>
//          <query xmlns=' http://jabber.org/protocol/disco#items'
//                 node='blah '/>
//      </iq>
//
// Sample response:
//
//      <iq type='result'
//          from=' hendriks@google.com'
//          to='rsturgell@google.com/asdf'
//          id='1234'>
//          <query xmlns=' http://jabber.org/protocol/disco#items '
//                 node='blah'>
//                 <item something='somethingelse'/>
//          </query>
//      </iq>


#ifndef TALK_XMPP_DISCOITEMSQUERYTASK_H_
#define TALK_XMPP_DISCOITEMSQUERYTASK_H_

#include <string>
#include <vector>

#include "talk/xmpp/iqtask.h"

namespace buzz {

struct DiscoItem {
  std::string jid;
  std::string node;
  std::string name;
};

class DiscoItemsQueryTask : public IqTask {
 public:
  DiscoItemsQueryTask(XmppTaskParentInterface* parent,
                      const Jid& to, const std::string& node);

  sigslot::signal1<std::vector<DiscoItem> > SignalResult;

 private:
  static XmlElement* MakeRequest(const std::string& node);
  virtual void HandleResult(const XmlElement* result);
  static bool ParseItem(const XmlElement* element, DiscoItem* item);
};

}  // namespace buzz

#endif  // TALK_XMPP_DISCOITEMSQUERYTASK_H_
