/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_JINGLEINFOTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_JINGLEINFOTASK_H_

#include <vector>

#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/base/sigslot.h"

namespace buzz {

class JingleInfoTask : public XmppTask {
 public:
  explicit JingleInfoTask(XmppTaskParentInterface* parent) :
    XmppTask(parent, XmppEngine::HL_TYPE) {}

  virtual int ProcessStart();
  void RefreshJingleInfoNow();

  sigslot::signal3<const std::string &,
                   const std::vector<std::string> &,
                   const std::vector<rtc::SocketAddress> &>
                       SignalJingleInfo;

 protected:
  class JingleInfoGetTask;
  friend class JingleInfoGetTask;

  virtual bool HandleStanza(const XmlElement * stanza);
};
}

#endif  // WEBRTC_LIBJINGLE_XMPP_JINGLEINFOTASK_H_
