/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/session/transportparser.h"

#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"

namespace cricket {

bool TransportParser::ParseAddress(const buzz::XmlElement* elem,
                                   const buzz::QName& address_name,
                                   const buzz::QName& port_name,
                                   rtc::SocketAddress* address,
                                   ParseError* error) {
  if (!elem->HasAttr(address_name))
    return BadParse("address does not have " + address_name.LocalPart(), error);
  if (!elem->HasAttr(port_name))
    return BadParse("address does not have " + port_name.LocalPart(), error);

  address->SetIP(elem->Attr(address_name));
  std::istringstream ist(elem->Attr(port_name));
  int port = 0;
  ist >> port;
  address->SetPort(port);

  return true;
}

}  // namespace cricket
