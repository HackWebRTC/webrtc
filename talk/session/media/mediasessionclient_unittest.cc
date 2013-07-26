/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/media/mediasessionclient.h"
#include "talk/xmllite/xmlbuilder.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlprinter.h"
#include "talk/xmpp/constants.h"

// The codecs that our FakeMediaEngine will support. Order is important, since
// the tests check that our messages have codecs in the correct order.
static const cricket::AudioCodec kAudioCodecs[] = {
  cricket::AudioCodec(103, "ISAC",   16000, -1,    1, 18),
  cricket::AudioCodec(104, "ISAC",   32000, -1,    1, 17),
  cricket::AudioCodec(119, "ISACLC", 16000, 40000, 1, 16),
  cricket::AudioCodec(99,  "speex",  16000, 22000, 1, 15),
  cricket::AudioCodec(97,  "IPCMWB", 16000, 80000, 1, 14),
  cricket::AudioCodec(9,   "G722",   16000, 64000, 1, 13),
  cricket::AudioCodec(102, "iLBC",   8000,  13300, 1, 12),
  cricket::AudioCodec(98,  "speex",  8000,  11000, 1, 11),
  cricket::AudioCodec(3,   "GSM",    8000,  13000, 1, 10),
  cricket::AudioCodec(100, "EG711U", 8000,  64000, 1, 9),
  cricket::AudioCodec(101, "EG711A", 8000,  64000, 1, 8),
  cricket::AudioCodec(0,   "PCMU",   8000,  64000, 1, 7),
  cricket::AudioCodec(8,   "PCMA",   8000,  64000, 1, 6),
  cricket::AudioCodec(126, "CN",     32000, 0,     1, 5),
  cricket::AudioCodec(105, "CN",     16000, 0,     1, 4),
  cricket::AudioCodec(13,  "CN",     8000,  0,     1, 3),
  cricket::AudioCodec(117, "red",    8000,  0,     1, 2),
  cricket::AudioCodec(106, "telephone-event", 8000, 0, 1, 1)
};

static const cricket::VideoCodec kVideoCodecs[] = {
  cricket::VideoCodec(96, "H264-SVC", 320, 200, 30, 1)
};

static const cricket::DataCodec kDataCodecs[] = {
  cricket::DataCodec(127, "google-data", 0)
};

const std::string kGingleCryptoOffer = \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1'>   "  \
    "  <usage/>                                                "  \
    "  <rtp:crypto tag='145' crypto-suite='AES_CM_128_HMAC_SHA1_32'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='AES_CM_128_HMAC_SHA1_80'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";

// Jingle offer does not have any <usage> element.
const std::string kJingleCryptoOffer = \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1'>   "  \
    "  <rtp:crypto tag='145' crypto-suite='AES_CM_128_HMAC_SHA1_32'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='AES_CM_128_HMAC_SHA1_80'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";


const std::string kGingleRequiredCryptoOffer = \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1' required='true'> "\
    "  <usage/>                                                "  \
    "  <rtp:crypto tag='145' crypto-suite='AES_CM_128_HMAC_SHA1_32'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='AES_CM_128_HMAC_SHA1_80'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";

const std::string kJingleRequiredCryptoOffer = \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1' required='true'> "\
    "  <rtp:crypto tag='145' crypto-suite='AES_CM_128_HMAC_SHA1_32'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='AES_CM_128_HMAC_SHA1_80'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";


const std::string kGingleUnsupportedCryptoOffer = \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1'>   "  \
    "  <usage/>                                                "  \
    "  <rtp:crypto tag='145' crypto-suite='NOT_SUPPORTED_1'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='NOT_SUPPORTED_2'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";

const std::string kJingleUnsupportedCryptoOffer =                 \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1'>   "  \
    "  <rtp:crypto tag='145' crypto-suite='NOT_SUPPORTED_1'" \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='NOT_SUPPORTED_2'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";


// With unsupported but with required="true"
const std::string kGingleRequiredUnsupportedCryptoOffer =         \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1' required='true'>" \
    "  <usage/>                                                "  \
    "  <rtp:crypto tag='145' crypto-suite='NOT_SUPPORTED_1'"      \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>" \
    "  <rtp:crypto tag='51' crypto-suite='NOT_SUPPORTED_2'" \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>" \
    "</rtp:encryption>                                         ";

const std::string kJingleRequiredUnsupportedCryptoOffer =                     \
    "<rtp:encryption xmlns:rtp='urn:xmpp:jingle:apps:rtp:1' required='true'>" \
    "  <rtp:crypto tag='145' crypto-suite='NOT_SUPPORTED_1'                 " \
    "  key-params='inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9'/>       " \
    "  <rtp:crypto tag='51' crypto-suite='NOT_SUPPORTED_2'                  " \
    "  key-params='inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy'/>"        \
    "</rtp:encryption>                                         ";

const std::string kGingleInitiate(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='104' name='ISAC' clockrate='32000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='119' name='ISACLC' clockrate='16000' bitrate='40000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='99' name='speex' clockrate='16000' bitrate='22000' />    " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='97' name='IPCMWB' clockrate='16000' bitrate='80000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='9' name='G722' clockrate='16000' bitrate='64000' /> " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='102' name='iLBC' clockrate='8000' bitrate='13300' />" \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='98' name='speex' clockrate='8000' bitrate='11000' />" \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='3' name='GSM' clockrate='8000' bitrate='13000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='100' name='EG711U' clockrate='8000' bitrate='64000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='101' name='EG711A' clockrate='8000' bitrate='64000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='0' name='PCMU' clockrate='8000' bitrate='64000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='8' name='PCMA' clockrate='8000' bitrate='64000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='126' name='CN' clockrate='32000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='105' name='CN' clockrate='16000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='13' name='CN' clockrate='8000' />                   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='117' name='red' clockrate='8000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='106' name='telephone-event' clockrate='8000' />     " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiate(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "        <payload-type id='104' name='ISAC' clockrate='32000'/>    " \
     "        <payload-type                                             " \
     "          id='119' name='ISACLC' clockrate='16000'>               " \
     "          <parameter name='bitrate' value='40000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='99' name='speex' clockrate='16000'>                 " \
     "          <parameter name='bitrate' value='22000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='97' name='IPCMWB' clockrate='16000'>                " \
     "          <parameter name='bitrate' value='80000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='9' name='G722' clockrate='16000'>                   " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='102' name='iLBC' clockrate='8000'>                  " \
     "          <parameter name='bitrate' value='13300'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='98' name='speex' clockrate='8000'>                  " \
     "          <parameter name='bitrate' value='11000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='3' name='GSM' clockrate='8000'>                     " \
     "          <parameter name='bitrate' value='13000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='100' name='EG711U' clockrate='8000'>                " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='101' name='EG711A' clockrate='8000'>                " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='0' name='PCMU' clockrate='8000'>                    " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='8' name='PCMA' clockrate='8000'>                    " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='126' name='CN' clockrate='32000' />                 " \
     "        <payload-type                                             " \
     "          id='105' name='CN' clockrate='16000' />                 " \
     "        <payload-type                                             " \
     "          id='13' name='CN' clockrate='8000' />                   " \
     "        <payload-type                                             " \
     "          id='117' name='red' clockrate='8000' />                 " \
     "        <payload-type                                             " \
     "          id='106' name='telephone-event' clockrate='8000' />     " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

// Initiate string with a different order of supported codecs.
// Should accept the supported ones, but with our desired order.
const std::string kGingleInitiateDifferentPreference(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='104' name='ISAC' clockrate='32000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='97' name='IPCMWB' clockrate='16000' bitrate='80000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='9' name='G722' clockrate='16000' bitrate='64000' /> " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='119' name='ISACLC' clockrate='16000' bitrate='40000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='99' name='speex' clockrate='16000' bitrate='22000' />    " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='100' name='EG711U' clockrate='8000' bitrate='64000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='101' name='EG711A' clockrate='8000' bitrate='64000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='0' name='PCMU' clockrate='8000' bitrate='64000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='8' name='PCMA' clockrate='8000' bitrate='64000' />  " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='102' name='iLBC' clockrate='8000' bitrate='13300' />" \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='3' name='GSM' clockrate='8000' bitrate='13000' />   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='98' name='speex' clockrate='8000' bitrate='11000' />" \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='126' name='CN' clockrate='32000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='105' name='CN' clockrate='16000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='13' name='CN' clockrate='8000' />                   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='117' name='red' clockrate='8000' />                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='106' name='telephone-event' clockrate='8000' />     " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateDifferentPreference(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='104' name='ISAC' clockrate='32000'/>    " \
     "        <payload-type                                             " \
     "          id='97' name='IPCMWB' clockrate='16000'>                " \
     "          <parameter name='bitrate' value='80000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='9' name='G722' clockrate='16000'>                   " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='119' name='ISACLC' clockrate='16000'>               " \
     "          <parameter name='bitrate' value='40000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "        <payload-type                                             " \
     "          id='99' name='speex' clockrate='16000'>                 " \
     "          <parameter name='bitrate' value='22000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='100' name='EG711U' clockrate='8000'>                " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='101' name='EG711A' clockrate='8000'>                " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='0' name='PCMU' clockrate='8000'>                    " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='8' name='PCMA' clockrate='8000'>                    " \
     "          <parameter name='bitrate' value='64000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='102' name='iLBC' clockrate='8000'>                  " \
     "          <parameter name='bitrate' value='13300'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='3' name='GSM' clockrate='8000'>                     " \
     "          <parameter name='bitrate' value='13000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='98' name='speex' clockrate='8000'>                  " \
     "          <parameter name='bitrate' value='11000'/>               " \
     "        </payload-type>                                           " \
     "        <payload-type                                             " \
     "          id='126' name='CN' clockrate='32000' />                 " \
     "        <payload-type                                             " \
     "          id='105' name='CN' clockrate='16000' />                 " \
     "        <payload-type                                             " \
     "          id='13' name='CN' clockrate='8000' />                   " \
     "        <payload-type                                             " \
     "          id='117' name='red' clockrate='8000' />                 " \
     "        <payload-type                                             " \
     "          id='106' name='telephone-event' clockrate='8000' />     " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

const std::string kGingleVideoInitiate(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/video'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/video' " \
     "        id='99' name='H264-SVC' framerate='30'                  " \
     "        height='200' width='320'/>                              " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleVideoInitiate(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test video'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='video'> " \
     "        <payload-type id='99' name='H264-SVC'>                    " \
     "          <parameter name='height' value='200'/>                  " \
     "          <parameter name='width' value='320'/>                   " \
     "          <parameter name='framerate' value='30'/>                " \
     "        </payload-type>                                           " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

const std::string kJingleVideoInitiateWithRtpData(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test video'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='video'> " \
     "        <payload-type id='99' name='H264-SVC'>                    " \
     "          <parameter name='height' value='200'/>                  " \
     "          <parameter name='width' value='320'/>                   " \
     "          <parameter name='framerate' value='30'/>                " \
     "        </payload-type>                                           " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test data'>                                    " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='data'> " \
     "        <payload-type id='127' name='google-data'/>               " \
     "        <rtcp-mux/>                                               " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

const std::string kJingleVideoInitiateWithSctpData(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test video'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='video'> " \
     "        <payload-type id='99' name='H264-SVC'>                    " \
     "          <parameter name='height' value='200'/>                  " \
     "          <parameter name='width' value='320'/>                   " \
     "          <parameter name='framerate' value='30'/>                " \
     "        </payload-type>                                           " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test data'>                                    " \
     "      <description xmlns='google:jingle:sctp' media='data'>       " \
     "        <stream sid='1'/>                                         " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

const std::string kJingleVideoInitiateWithBandwidth(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "         sid='abcdef' initiator='me@domain.com/resource'>         " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test video'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='video'> " \
     "        <payload-type id='99' name='H264-SVC'>                    " \
     "          <parameter name='height' value='200'/>                  " \
     "          <parameter name='width' value='320'/>                   " \
     "          <parameter name='framerate' value='30'/>                " \
     "        </payload-type>                                           " \
     "        <bandwidth type='AS'>42</bandwidth>                       " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

const std::string kJingleVideoInitiateWithRtcpMux(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "         sid='abcdef' initiator='me@domain.com/resource'>         " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "    <content name='test video'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='video'> " \
     "        <payload-type id='99' name='H264-SVC'>                    " \
     "          <parameter name='height' value='200'/>                  " \
     "          <parameter name='width' value='320'/>                   " \
     "          <parameter name='framerate' value='30'/>                " \
     "        </payload-type>                                           " \
     "        <rtcp-mux/>                                               " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

// Initiate string with a combination of supported and unsupported codecs
// Should accept the supported ones
const std::string kGingleInitiateSomeUnsupported(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='97' name='ASDFDS' />                                " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='102' name='1010' />                                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='107' name='DFAS' />                                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='100' name='EG711U' />                               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='101' name='EG711A' />                               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='0' name='PCMU' />                                   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='110' name=':)' />                                   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='13' name='CN' />                                    " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateSomeUnsupported(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'   " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'> " \
     "        <payload-type                                           " \
     "          id='103' name='ISAC' clockrate='16000' />             " \
     "        <payload-type                                           " \
     "          id='97' name='ASDFDS' />                              " \
     "        <payload-type                                           " \
     "          id='102' name='1010' />                               " \
     "        <payload-type                                           " \
     "          id='107' name='DFAS' />                               " \
     "        <payload-type                                           " \
     "          id='100' name='EG711U' />                             " \
     "        <payload-type                                           " \
     "          id='101' name='EG711A' />                             " \
     "        <payload-type                                           " \
     "          id='0' name='PCMU' />                                 " \
     "        <payload-type                                           " \
     "          id='110' name=':)' />                                 " \
     "        <payload-type                                           " \
     "          id='13' name='CN' />                                  " \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/> " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

const std::string kGingleVideoInitiateWithBandwidth(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/video'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/video' " \
     "        id='99' name='H264-SVC' framerate='30'                  " \
     "        height='200' width='320'/>                              " \
     "      <bandwidth type='AS'>42</bandwidth>                       " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

// Initiate string without any supported codecs. Should send a reject.
const std::string kGingleInitiateNoSupportedAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='123' name='Supercodec6000' />                       " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateNoSupportedAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'   " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type                                           " \
     "          id='123' name='Supercodec6000' />                     " \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>  " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

// Initiate string without any codecs. Assumes ancient version of Cricket
// and tries a session with ISAC and PCMU
const std::string kGingleInitiateNoAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateNoAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'   " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>  " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

// The codecs are supported, but not at the given clockrates. Should send
// a reject.
const std::string kGingleInitiateWrongClockrates(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='8000'/>                 " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='97' name='IPCMWB' clockrate='1337'/>                " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='102' name='iLBC' clockrate='1982' />                " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateWrongClockrates(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type                                             " \
     "          id='103' name='ISAC' clockrate='8000'/>                 " \
     "        <payload-type                                             " \
     "          id='97' name='IPCMWB' clockrate='1337'/>                " \
     "       <payload-type                                              " \
     "          id='102' name='iLBC' clockrate='1982' />                " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>  " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

// The codecs are supported, but not with the given number of channels.
// Should send a reject.
const std::string kGingleInitiateWrongChannels(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' channels='2'/>                     " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='97' name='IPCMWB' channels='3'/>                    " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateWrongChannels(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'>    " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type                                             " \
     "          id='103' name='ISAC' channels='2'/>                     " \
     "        <payload-type                                             " \
     "          id='97' name='IPCMWB' channels='3'/>                    " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

// Initiate with a dynamic codec not using webrtc default payload id. Should
// accept with provided payload id.
const std::string kGingleInitiateDynamicAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='123' name='speex' clockrate='16000'/>               " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateDynamicAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'           " \
     "    to='user@domain.com/resource' type='set' id='123'>            " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'     " \
     "          sid='abcdef' initiator='me@domain.com/resource'>        " \
     "    <content name='test audio'>                                   " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type                                             " \
     "          id='123' name='speex' clockrate='16000'/>               " \
     "      </description>                                              " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>   " \
     "    </content>                                                    " \
     "  </jingle>                                                       " \
     "</iq>                                                             ");

// Initiate string with nothing but static codec id's. Should accept.
const std::string kGingleInitiateStaticAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='3' />                                               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='0' />                                               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='8' />                                               " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateStaticAudioCodecs(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'   " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type id='3' />                                 " \
     "        <payload-type id='0' />                                 " \
     "        <payload-type id='8' />                                 " \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/> " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

// Initiate with payload type-less codecs. Should reject.
const std::string kGingleInitiateNoPayloadTypes(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "       name='ISAC' clockrate='16000'/>                          " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateNoPayloadTypes(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'>  " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type  name='ISAC' clockrate='16000'/>          " \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/> " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

// Initiate with unnamed dynamic codces. Should reject.
const std::string kGingleInitiateDynamicWithoutNames(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <session xmlns='http://www.google.com/session' type='initiate'" \
     "    id='abcdef' initiator='me@domain.com/resource'>             " \
     "    <description xmlns='http://www.google.com/session/phone'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "       id='100' clockrate='16000'/>                             " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleInitiateDynamicWithoutNames(
     "<iq xmlns='jabber:client' from='me@domain.com/resource'         " \
     "    to='user@domain.com/resource' type='set' id='123'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-initiate'>  " \
     "          sid='abcdef' initiator='me@domain.com/resource'>      " \
     "    <content name='test audio'>                                 " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>" \
     "        <payload-type id='100' clockrate='16000'/>              " \
     "      </description>                                            " \
     "     <transport xmlns=\"http://www.google.com/transport/p2p\"/>  " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

const uint32 kAudioSsrc = 4294967295U;
const uint32 kVideoSsrc = 87654321;
const uint32 kDataSsrc = 1010101;
const uint32 kDataSid = 0;
// Note that this message does not specify a session ID. It must be populated
// before use.
const std::string kGingleAcceptWithSsrcs(
     "<iq xmlns='jabber:client' from='me@mydomain.com'                " \
     "    to='user@domain.com/resource' type='set' id='150'>          " \
     "  <session xmlns='http://www.google.com/session' type='accept'  " \
     "    initiator='me@domain.com/resource'>                         " \
     "    <description xmlns='http://www.google.com/session/video'>   " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='103' name='ISAC' clockrate='16000' />               " \
     "      <payload-type xmlns='http://www.google.com/session/phone' " \
     "        id='104' name='ISAC' clockrate='32000' />               " \
     "      <src-id xmlns='http://www.google.com/session/phone'>      " \
     "        4294967295</src-id>                                       " \
     "      <src-id>87654321</src-id>                                 " \
     "    </description>                                              " \
     "  </session>                                                    " \
     "</iq>                                                           ");

const std::string kJingleAcceptWithSsrcs(
     "<iq xmlns='jabber:client' from='me@mydomain.com'                " \
     "    to='user@domain.com/resource' type='set' id='150'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-accept'     " \
     "          initiator='me@domain.com/resource'>                   " \
     "    <content name='audio'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='audio' ssrc='4294967295'>                      " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>  " \
     "        <payload-type id='104' name='ISAC' clockrate='32000'/>  " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "    <content name='video'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='video' ssrc='87654321'>                        " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

const std::string kJingleAcceptWithRtpDataSsrcs(
     "<iq xmlns='jabber:client' from='me@mydomain.com'                " \
     "    to='user@domain.com/resource' type='set' id='150'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-accept'     " \
     "          initiator='me@domain.com/resource'>                   " \
     "    <content name='audio'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='audio' ssrc='4294967295'>                      " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>  " \
     "        <payload-type id='104' name='ISAC' clockrate='32000'/>  " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "    <content name='video'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='video' ssrc='87654321'>                        " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "    <content name='data'>                                       " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='data' ssrc='1010101'>                          " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

const std::string kJingleAcceptWithSctpData(
     "<iq xmlns='jabber:client' from='me@mydomain.com'                " \
     "    to='user@domain.com/resource' type='set' id='150'>          " \
     "  <jingle xmlns='urn:xmpp:jingle:1' action='session-accept'     " \
     "          initiator='me@domain.com/resource'>                   " \
     "    <content name='audio'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='audio' ssrc='4294967295'>                      " \
     "        <payload-type id='103' name='ISAC' clockrate='16000'/>  " \
     "        <payload-type id='104' name='ISAC' clockrate='32000'/>  " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "    <content name='video'>                                      " \
     "      <description xmlns='urn:xmpp:jingle:apps:rtp:1'           " \
     "          media='video' ssrc='87654321'>                        " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "    <content name='data'>                                       " \
     "      <description xmlns='google:jingle:sctp'>                  " \
     "        <stream sid='1'/>                                       " \
     "      </description>                                            " \
     "     <transport xmlns='http://www.google.com/transport/p2p'/>   " \
     "    </content>                                                  " \
     "  </jingle>                                                     " \
     "</iq>                                                           ");

std::string JingleView(const std::string& ssrc,
                       const std::string& width,
                       const std::string& height,
                       const std::string& framerate) {
  // We have some slightly weird whitespace formatting to make the
  // actual XML generated match the expected XML here.
  return \
      "<cli:iq"
      "  to='me@mydomain.com'"
      "  type='set'"
      "  xmlns:cli='jabber:client'>"
        "<jingle"
      "    xmlns='urn:xmpp:jingle:1'"
      "    action='session-info'"
      "    sid=''>"
          "<view xmlns='google:jingle'"
      "      name='video'"
      "      type='static'"
      "      ssrc='" + ssrc + "'>"
            "<params"
      "        width='" + width + "'"
      "        height='" + height + "'"
      "        framerate='" + framerate + "'"
      "        preference='0'/>"
          "</view>"
        "</jingle>"
      "</cli:iq>";
}

std::string JingleStreamAdd(const std::string& content_name,
                            const std::string& nick,
                            const std::string& name,
                            const std::string& ssrc) {
  return \
      "<iq"
      "  xmlns='jabber:client'"
      "  from='me@mydomain.com'"
      "  to='user@domain.com/resource'"
      "  type='set'"
      "  id='150'>"
      "  <jingle"
      "    xmlns='urn:xmpp:jingle:1'"
      "    action='description-info'>"
      "    <content"
      "      xmlns='urn:xmpp:jingle:1'"
      "      name='" + content_name + "'>"
      "      <description"
      "        xmlns='urn:xmpp:jingle:apps:rtp:1'"
      "        media='" + content_name + "'>"
      "        <streams"
      "          xmlns='google:jingle'>"
      "          <stream"
      "            nick='" + nick + "'"
      "            name='" + name + "'>"
      "            <ssrc>"  + ssrc + "</ssrc>"
      "          </stream>"
      "        </streams>"
      "      </description>"
      "    </content>"
      "  </jingle>"
      "</iq>";
}

std::string JingleOutboundStreamRemove(const std::string& sid,
                                       const std::string& content_name,
                                       const std::string& name) {
  return \
      "<cli:iq"
      " to='me@mydomain.com'"
      " type='set'"
      " xmlns:cli='jabber:client'>"
      "<jingle"
      " xmlns='urn:xmpp:jingle:1'"
      " action='description-info'"
      " sid='" + sid + "'>"
      "<content"
      " name='" + content_name + "'"
      " creator='initiator'>"
      "<description"
      " xmlns='urn:xmpp:jingle:apps:rtp:1'"
      " media='" + content_name + "'>"
      "<streams"
      " xmlns='google:jingle'>"
      "<stream"
      " name='" + name + "'>"
      "</stream>"
      "</streams>"
      "</description>"
      "</content>"
      "</jingle>"
      "</cli:iq>";
}

std::string JingleOutboundStreamAdd(const std::string& sid,
                                    const std::string& content_name,
                                    const std::string& name,
                                    const std::string& ssrc) {
  return \
      "<cli:iq"
      " to='me@mydomain.com'"
      " type='set'"
      " xmlns:cli='jabber:client'>"
      "<jingle"
      " xmlns='urn:xmpp:jingle:1'"
      " action='description-info'"
      " sid='" + sid + "'>"
      "<content"
      " name='" + content_name + "'"
      " creator='initiator'>"
      "<description"
      " xmlns='urn:xmpp:jingle:apps:rtp:1'"
      " media='" + content_name + "'>"
      "<streams"
      " xmlns='google:jingle'>"
      "<stream"
      " name='" + name + "'>"
      "<ssrc>" + ssrc + "</ssrc>"
      "</stream>"
      "</streams>"
      "</description>"
      "</content>"
      "</jingle>"
      "</cli:iq>";
}

std::string JingleStreamAddWithoutSsrc(const std::string& content_name,
                                       const std::string& nick,
                                       const std::string& name) {
  return \
      "<iq"
      "  xmlns='jabber:client'"
      "  from='me@mydomain.com'"
      "  to='user@domain.com/resource'"
      "  type='set'"
      "  id='150'>"
      "  <jingle"
      "    xmlns='urn:xmpp:jingle:1'"
      "    action='description-info'>"
      "    <content"
      "      xmlns='urn:xmpp:jingle:1'"
      "      name='" + content_name + "'>"
      "      <description"
      "        xmlns='urn:xmpp:jingle:apps:rtp:1'"
      "        media='" + content_name + "'>"
      "        <streams"
      "          xmlns='google:jingle'>"
      "          <stream"
      "            nick='" + nick + "'"
      "            name='" + name + "'>"
       "          </stream>"
      "        </streams>"
      "      </description>"
      "    </content>"
      "  </jingle>"
      "</iq>";
}

std::string JingleStreamRemove(const std::string& content_name,
                               const std::string& nick,
                               const std::string& name) {
  return \
      "<iq"
      "  xmlns='jabber:client'"
      "  from='me@mydomain.com'"
      "  to='user@domain.com/resource'"
      "  type='set'"
      "  id='150'>"
      "  <jingle"
      "    xmlns='urn:xmpp:jingle:1'"
      "    action='description-info'>"
      "    <content"
      "      xmlns='urn:xmpp:jingle:1'"
      "      name='" + content_name + "'>"
      "      <description"
      "        xmlns='urn:xmpp:jingle:apps:rtp:1'"
      "        media='" + content_name + "'>"
      "        <streams"
      "          xmlns='google:jingle'>"
      "          <stream"
      "            nick='" + nick + "'"
      "            name='" + name + "'/>"
      "        </streams>"
      "      </description>"
      "    </content>"
      "  </jingle>"
      "</iq>";
}

// Convenience function to get CallOptions that have audio enabled,
// but not video or data.
static cricket::CallOptions AudioCallOptions() {
  cricket::CallOptions options;
  options.has_audio = true;
  options.has_video = false;
  options.data_channel_type = cricket::DCT_NONE;
  return options;
}

// Convenience function to get CallOptions that have audio and video
// enabled, but not data.
static cricket::CallOptions VideoCallOptions() {
  cricket::CallOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_NONE;
  return options;
}

buzz::XmlElement* CopyElement(const buzz::XmlElement* elem) {
  return new buzz::XmlElement(*elem);
}

static std::string AddEncryption(std::string stanza, std::string encryption) {
  std::string::size_type pos = stanza.find("</description>");
  while (pos != std::string::npos) {
      stanza = stanza.insert(pos, encryption);
      pos = stanza.find("</description>", pos + encryption.length() + 1);
  }
  return stanza;
}

int IntFromJingleCodecParameter(const buzz::XmlElement* parameter,
                                const std::string& expected_name) {
  if (parameter) {
    const std::string& actual_name =
        parameter->Attr(cricket::QN_PAYLOADTYPE_PARAMETER_NAME);

    EXPECT_EQ(expected_name, actual_name)
        << "wrong parameter name.  Expected '"
        << expected_name << "'. Actually '"
        << actual_name << "'.";

    return atoi(parameter->Attr(
        cricket::QN_PAYLOADTYPE_PARAMETER_VALUE).c_str());
  }
  return 0;
}

// Parses and extracts payload and codec info from test XML.  Since
// that XML will be in various contents (Gingle and Jingle), we need an
// abstract parser with one concrete implementation per XML content.
class MediaSessionTestParser {
 public:
  virtual buzz::XmlElement* ActionFromStanza(buzz::XmlElement* stanza) = 0;
  virtual buzz::XmlElement* ContentFromAction(buzz::XmlElement* action) = 0;
  virtual buzz::XmlElement* NextContent(buzz::XmlElement* content) = 0;
  virtual buzz::XmlElement* PayloadTypeFromContent(
      buzz::XmlElement* content) = 0;
  virtual buzz::XmlElement* NextFromPayloadType(
      buzz::XmlElement* payload_type) = 0;
  virtual cricket::AudioCodec AudioCodecFromPayloadType(
      const buzz::XmlElement* payload_type) = 0;
  virtual cricket::VideoCodec VideoCodecFromPayloadType(
      const buzz::XmlElement* payload_type) = 0;
  virtual cricket::DataCodec DataCodecFromPayloadType(
      const buzz::XmlElement* payload_type) = 0;
  virtual buzz::XmlElement* EncryptionFromContent(
      buzz::XmlElement* content) = 0;
  virtual buzz::XmlElement* NextFromEncryption(
      buzz::XmlElement* encryption) = 0;
  virtual const buzz::XmlElement* BandwidthFromContent(
      buzz::XmlElement* content) = 0;
  virtual const buzz::XmlElement* RtcpMuxFromContent(
      buzz::XmlElement* content) = 0;
  virtual bool ActionIsTerminate(const buzz::XmlElement* action) = 0;
  virtual ~MediaSessionTestParser() {}
};

class JingleSessionTestParser : public MediaSessionTestParser {
 public:
  JingleSessionTestParser() {}

  ~JingleSessionTestParser() {
  }

  buzz::XmlElement* ActionFromStanza(buzz::XmlElement* stanza) {
    return stanza->FirstNamed(cricket::QN_JINGLE);
  }

  buzz::XmlElement* ContentFromAction(buzz::XmlElement* action) {
    // We need to be able to use multiple contents, but the action
    // gets deleted before we can call NextContent, so we need to
    // stash away a copy.
    action_.reset(CopyElement(action));
    return action_->FirstNamed(cricket::QN_JINGLE_CONTENT);
  }

  buzz::XmlElement* NextContent(buzz::XmlElement* content) {
    // For some reason, content->NextNamed(cricket::QN_JINGLE_CONTENT)
    // doesn't work.
    return action_->FirstNamed(cricket::QN_JINGLE_CONTENT)
        ->NextNamed(cricket::QN_JINGLE_CONTENT);
  }

  buzz::XmlElement* PayloadTypeFromContent(buzz::XmlElement* content) {
    buzz::XmlElement* content_desc =
        content->FirstNamed(cricket::QN_JINGLE_RTP_CONTENT);
    if (!content_desc)
      return NULL;

    return content_desc->FirstNamed(cricket::QN_JINGLE_RTP_PAYLOADTYPE);
  }

  buzz::XmlElement* NextFromPayloadType(buzz::XmlElement* payload_type) {
    return payload_type->NextNamed(cricket::QN_JINGLE_RTP_PAYLOADTYPE);
  }

  cricket::AudioCodec AudioCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    int id = 0;
    if (payload_type->HasAttr(cricket::QN_ID))
      id = atoi(payload_type->Attr(cricket::QN_ID).c_str());

    std::string name;
    if (payload_type->HasAttr(cricket::QN_NAME))
      name = payload_type->Attr(cricket::QN_NAME);

    int clockrate = 0;
    if (payload_type->HasAttr(cricket::QN_CLOCKRATE))
      clockrate = atoi(payload_type->Attr(cricket::QN_CLOCKRATE).c_str());

    int bitrate = IntFromJingleCodecParameter(
        payload_type->FirstNamed(cricket::QN_PARAMETER), "bitrate");

    int channels = 1;
    if (payload_type->HasAttr(cricket::QN_CHANNELS))
      channels = atoi(payload_type->Attr(
          cricket::QN_CHANNELS).c_str());

    return cricket::AudioCodec(id, name, clockrate, bitrate, channels, 0);
  }

  cricket::VideoCodec VideoCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    int id = 0;
    if (payload_type->HasAttr(cricket::QN_ID))
      id = atoi(payload_type->Attr(cricket::QN_ID).c_str());

    std::string name;
    if (payload_type->HasAttr(cricket::QN_NAME))
      name = payload_type->Attr(cricket::QN_NAME);

    int width = 0;
    int height = 0;
    int framerate = 0;
    const buzz::XmlElement* param =
        payload_type->FirstNamed(cricket::QN_PARAMETER);
    if (param) {
      width = IntFromJingleCodecParameter(param, "width");
      param = param->NextNamed(cricket::QN_PARAMETER);
      if (param) {
        height = IntFromJingleCodecParameter(param, "height");
        param = param->NextNamed(cricket::QN_PARAMETER);
        if (param) {
          framerate = IntFromJingleCodecParameter(param, "framerate");
        }
      }
    }

    return cricket::VideoCodec(id, name, width, height, framerate, 0);
  }

  cricket::DataCodec DataCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    int id = 0;
    if (payload_type->HasAttr(cricket::QN_ID))
      id = atoi(payload_type->Attr(cricket::QN_ID).c_str());

    std::string name;
    if (payload_type->HasAttr(cricket::QN_NAME))
      name = payload_type->Attr(cricket::QN_NAME);

    return cricket::DataCodec(id, name, 0);
  }

  bool ActionIsTerminate(const buzz::XmlElement* action) {
    return (action->HasAttr(cricket::QN_ACTION) &&
            action->Attr(cricket::QN_ACTION) == "session-terminate");
  }

  buzz::XmlElement* EncryptionFromContent(buzz::XmlElement* content) {
    buzz::XmlElement* content_desc =
        content->FirstNamed(cricket::QN_JINGLE_RTP_CONTENT);
    if (!content_desc)
      return NULL;

    return content_desc->FirstNamed(cricket::QN_ENCRYPTION);
  }

  buzz::XmlElement* NextFromEncryption(buzz::XmlElement* encryption) {
    return encryption->NextNamed(cricket::QN_ENCRYPTION);
  }

  const buzz::XmlElement* BandwidthFromContent(buzz::XmlElement* content) {
    buzz::XmlElement* content_desc =
        content->FirstNamed(cricket::QN_JINGLE_RTP_CONTENT);
    if (!content_desc)
      return NULL;

    return content_desc->FirstNamed(cricket::QN_JINGLE_RTP_BANDWIDTH);
  }

  const buzz::XmlElement* RtcpMuxFromContent(buzz::XmlElement* content) {
    return content->FirstNamed(cricket::QN_JINGLE_RTCP_MUX);
  }

 private:
  talk_base::scoped_ptr<buzz::XmlElement> action_;
};

class GingleSessionTestParser : public MediaSessionTestParser {
 public:
  GingleSessionTestParser() : found_content_count_(0) {}

  buzz::XmlElement* ActionFromStanza(buzz::XmlElement* stanza) {
    return stanza->FirstNamed(cricket::QN_GINGLE_SESSION);
  }

  buzz::XmlElement* ContentFromAction(buzz::XmlElement* session) {
    buzz::XmlElement* content =
        session->FirstNamed(cricket::QN_GINGLE_AUDIO_CONTENT);
    if (content == NULL)
      content = session->FirstNamed(cricket::QN_GINGLE_VIDEO_CONTENT);
    return content;
  }

  // Assumes contents are in order of audio, and then video.
  buzz::XmlElement* NextContent(buzz::XmlElement* content) {
    found_content_count_++;
    return content;
  }

  buzz::XmlElement* PayloadTypeFromContent(buzz::XmlElement* content) {
    if (found_content_count_ > 0) {
      return content->FirstNamed(cricket::QN_GINGLE_VIDEO_PAYLOADTYPE);
    } else {
      return content->FirstNamed(cricket::QN_GINGLE_AUDIO_PAYLOADTYPE);
    }
  }

  buzz::XmlElement* NextFromPayloadType(buzz::XmlElement* payload_type) {
    if (found_content_count_ > 0) {
      return payload_type->NextNamed(cricket::QN_GINGLE_VIDEO_PAYLOADTYPE);
    } else {
      return payload_type->NextNamed(cricket::QN_GINGLE_AUDIO_PAYLOADTYPE);
    }
  }

  cricket::AudioCodec AudioCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    int id = 0;
    if (payload_type->HasAttr(cricket::QN_ID))
      id = atoi(payload_type->Attr(cricket::QN_ID).c_str());

    std::string name;
    if (payload_type->HasAttr(cricket::QN_NAME))
      name = payload_type->Attr(cricket::QN_NAME);

    int clockrate = 0;
    if (payload_type->HasAttr(cricket::QN_CLOCKRATE))
      clockrate = atoi(payload_type->Attr(cricket::QN_CLOCKRATE).c_str());

    int bitrate = 0;
    if (payload_type->HasAttr(cricket::QN_BITRATE))
      bitrate = atoi(payload_type->Attr(cricket::QN_BITRATE).c_str());

    int channels = 1;
    if (payload_type->HasAttr(cricket::QN_CHANNELS))
      channels = atoi(payload_type->Attr(cricket::QN_CHANNELS).c_str());

    return cricket::AudioCodec(id, name, clockrate, bitrate, channels, 0);
  }

  cricket::VideoCodec VideoCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    int id = 0;
    if (payload_type->HasAttr(cricket::QN_ID))
      id = atoi(payload_type->Attr(cricket::QN_ID).c_str());

    std::string name;
    if (payload_type->HasAttr(cricket::QN_NAME))
      name = payload_type->Attr(cricket::QN_NAME);

    int width = 0;
    if (payload_type->HasAttr(cricket::QN_WIDTH))
      width = atoi(payload_type->Attr(cricket::QN_WIDTH).c_str());

    int height = 0;
    if (payload_type->HasAttr(cricket::QN_HEIGHT))
      height = atoi(payload_type->Attr(cricket::QN_HEIGHT).c_str());

    int framerate = 1;
    if (payload_type->HasAttr(cricket::QN_FRAMERATE))
      framerate = atoi(payload_type->Attr(cricket::QN_FRAMERATE).c_str());

    return cricket::VideoCodec(id, name, width, height, framerate, 0);
  }

  cricket::DataCodec DataCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    // Gingle can't do data codecs.
    return cricket::DataCodec(0, "", 0);
  }

  buzz::XmlElement* EncryptionFromContent(
      buzz::XmlElement* content) {
    return content->FirstNamed(cricket::QN_ENCRYPTION);
  }

  buzz::XmlElement* NextFromEncryption(buzz::XmlElement* encryption) {
    return encryption->NextNamed(cricket::QN_ENCRYPTION);
  }

  const buzz::XmlElement* BandwidthFromContent(buzz::XmlElement* content) {
    return content->FirstNamed(cricket::QN_GINGLE_VIDEO_BANDWIDTH);
  }

  const buzz::XmlElement* RtcpMuxFromContent(buzz::XmlElement* content) {
    return NULL;
  }

  bool ActionIsTerminate(const buzz::XmlElement* session) {
    return (session->HasAttr(buzz::QN_TYPE) &&
            session->Attr(buzz::QN_TYPE) == "terminate");
  }

  int found_content_count_;
};

class MediaSessionClientTest : public sigslot::has_slots<> {
 public:
  explicit MediaSessionClientTest(MediaSessionTestParser* parser,
                                  cricket::SignalingProtocol initial_protocol) {
    nm_ = new talk_base::BasicNetworkManager();
    pa_ = new cricket::BasicPortAllocator(nm_);
    sm_ = new cricket::SessionManager(pa_, NULL);
    fme_ = new cricket::FakeMediaEngine();
    fdme_ = new cricket::FakeDataEngine();

    std::vector<cricket::AudioCodec>
        audio_codecs(kAudioCodecs, kAudioCodecs + ARRAY_SIZE(kAudioCodecs));
    fme_->SetAudioCodecs(audio_codecs);
    std::vector<cricket::VideoCodec>
        video_codecs(kVideoCodecs, kVideoCodecs + ARRAY_SIZE(kVideoCodecs));
    fme_->SetVideoCodecs(video_codecs);
    std::vector<cricket::DataCodec>
        data_codecs(kDataCodecs, kDataCodecs + ARRAY_SIZE(kDataCodecs));
    fdme_->SetDataCodecs(data_codecs);

    client_ = new cricket::MediaSessionClient(
        buzz::Jid("user@domain.com/resource"), sm_,
        fme_, fdme_, new cricket::FakeDeviceManager());
    client_->session_manager()->SignalOutgoingMessage.connect(
        this, &MediaSessionClientTest::OnSendStanza);
    client_->session_manager()->SignalSessionCreate.connect(
        this, &MediaSessionClientTest::OnSessionCreate);
    client_->SignalCallCreate.connect(
        this, &MediaSessionClientTest::OnCallCreate);
    client_->SignalCallDestroy.connect(
        this, &MediaSessionClientTest::OnCallDestroy);

    call_ = NULL;
    parser_ = parser;
    initial_protocol_ = initial_protocol;
    expect_incoming_crypto_ = false;
    expect_outgoing_crypto_ = false;
    expected_video_bandwidth_ = cricket::kAutoBandwidth;
    expected_video_rtcp_mux_ = false;
  }

  ~MediaSessionClientTest() {
    delete client_;
    delete sm_;
    delete pa_;
    delete nm_;
    delete parser_;
    ClearStanzas();
  }

  buzz::XmlElement* ActionFromStanza(buzz::XmlElement* stanza) {
    return parser_->ActionFromStanza(stanza);
  }

  buzz::XmlElement* ContentFromAction(buzz::XmlElement* action) {
    return parser_->ContentFromAction(action);
  }

  buzz::XmlElement* PayloadTypeFromContent(buzz::XmlElement* payload) {
    return parser_->PayloadTypeFromContent(payload);
  }

  buzz::XmlElement* NextFromPayloadType(buzz::XmlElement* payload_type) {
    return parser_->NextFromPayloadType(payload_type);
  }

  buzz::XmlElement* EncryptionFromContent(buzz::XmlElement* content) {
    return parser_->EncryptionFromContent(content);
  }

  buzz::XmlElement* NextFromEncryption(buzz::XmlElement* encryption) {
    return parser_->NextFromEncryption(encryption);
  }

  cricket::AudioCodec AudioCodecFromPayloadType(
      const buzz::XmlElement* payload_type) {
    return parser_->AudioCodecFromPayloadType(payload_type);
  }

  const cricket::AudioContentDescription* GetFirstAudioContentDescription(
      const cricket::SessionDescription* sdesc) {
    const cricket::ContentInfo* content =
        cricket::GetFirstAudioContent(sdesc);
    if (content == NULL)
      return NULL;
    return static_cast<const cricket::AudioContentDescription*>(
        content->description);
  }

  const cricket::VideoContentDescription* GetFirstVideoContentDescription(
      const cricket::SessionDescription* sdesc) {
    const cricket::ContentInfo* content =
        cricket::GetFirstVideoContent(sdesc);
    if (content == NULL)
      return NULL;
    return static_cast<const cricket::VideoContentDescription*>(
        content->description);
  }

  void CheckCryptoFromGoodIncomingInitiate(const cricket::Session* session) {
    ASSERT_TRUE(session != NULL);
    const cricket::AudioContentDescription* content =
        GetFirstAudioContentDescription(session->remote_description());
    ASSERT_TRUE(content != NULL);
    ASSERT_EQ(2U, content->cryptos().size());
    ASSERT_EQ(145, content->cryptos()[0].tag);
    ASSERT_EQ("AES_CM_128_HMAC_SHA1_32", content->cryptos()[0].cipher_suite);
    ASSERT_EQ("inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9",
              content->cryptos()[0].key_params);
    ASSERT_EQ(51, content->cryptos()[1].tag);
    ASSERT_EQ("AES_CM_128_HMAC_SHA1_80", content->cryptos()[1].cipher_suite);
    ASSERT_EQ("inline:J4lfdUL8W1F7TNJKcbuygaQuA429SJy2e9JctPUy",
              content->cryptos()[1].key_params);
  }

  void CheckCryptoForGoodOutgoingAccept(const cricket::Session* session) {
    const cricket::AudioContentDescription* content =
        GetFirstAudioContentDescription(session->local_description());
    ASSERT_EQ(1U, content->cryptos().size());
    ASSERT_EQ(145, content->cryptos()[0].tag);
    ASSERT_EQ("AES_CM_128_HMAC_SHA1_32", content->cryptos()[0].cipher_suite);
    ASSERT_EQ(47U, content->cryptos()[0].key_params.size());
  }

  void CheckBadCryptoFromIncomingInitiate(const cricket::Session* session) {
    const cricket::AudioContentDescription* content =
        GetFirstAudioContentDescription(session->remote_description());
    ASSERT_EQ(1U, content->cryptos().size());
    ASSERT_EQ(145, content->cryptos()[0].tag);
    ASSERT_EQ("NOT_SUPPORTED", content->cryptos()[0].cipher_suite);
    ASSERT_EQ("inline:hsWuSQJxx7przmb8HM+ZkeNcG3HezSNID7LmfDa9",
              content->cryptos()[0].key_params);
  }

  void CheckNoCryptoForOutgoingAccept(const cricket::Session* session) {
    const cricket::AudioContentDescription* content =
        GetFirstAudioContentDescription(session->local_description());
    ASSERT_TRUE(content->cryptos().empty());
  }

  void CheckVideoBandwidth(int expected_bandwidth,
                           const cricket::SessionDescription* sdesc) {
    const cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(sdesc);
    if (video != NULL) {
      ASSERT_EQ(expected_bandwidth, video->bandwidth());
    }
  }

  void CheckVideoRtcpMux(bool expected_video_rtcp_mux,
                         const cricket::SessionDescription* sdesc) {
    const cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(sdesc);
    if (video != NULL) {
      ASSERT_EQ(expected_video_rtcp_mux, video->rtcp_mux());
    }
  }

  virtual void CheckRtpDataContent(buzz::XmlElement* content) {
    if (initial_protocol_) {
      // Gingle can not write out data content.
      return;
    }

    buzz::XmlElement* e = PayloadTypeFromContent(content);
    ASSERT_TRUE(e != NULL);
    cricket::DataCodec codec = parser_->DataCodecFromPayloadType(e);
    EXPECT_EQ(127, codec.id);
    EXPECT_EQ("google-data", codec.name);

    CheckDataRtcpMux(true, call_->sessions()[0]->local_description());
    CheckDataRtcpMux(true, call_->sessions()[0]->remote_description());
    if (expect_outgoing_crypto_) {
      content = parser_->NextContent(content);
      buzz::XmlElement* encryption = EncryptionFromContent(content);
      ASSERT_TRUE(encryption != NULL);
      // TODO(pthatcher): Check encryption parameters?
    }
  }

  virtual void CheckSctpDataContent(buzz::XmlElement* content) {
    if (initial_protocol_) {
      // Gingle can not write out data content.
      return;
    }

    buzz::XmlElement* payload_type = PayloadTypeFromContent(content);
    ASSERT_TRUE(payload_type == NULL);
    buzz::XmlElement* encryption = EncryptionFromContent(content);
    ASSERT_TRUE(encryption == NULL);
    // TODO(pthatcher): Check for <streams>.
  }

  void CheckDataRtcpMux(bool expected_data_rtcp_mux,
                        const cricket::SessionDescription* sdesc) {
    const cricket::DataContentDescription* data =
        GetFirstDataContentDescription(sdesc);
    if (data != NULL) {
      ASSERT_EQ(expected_data_rtcp_mux, data->rtcp_mux());
    }
  }

  void CheckAudioSsrcForIncomingAccept(const cricket::Session* session) {
    const cricket::AudioContentDescription* audio =
        GetFirstAudioContentDescription(session->remote_description());
    ASSERT_TRUE(audio != NULL);
    ASSERT_EQ(kAudioSsrc, audio->first_ssrc());
  }

  void CheckVideoSsrcForIncomingAccept(const cricket::Session* session) {
    const cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(session->remote_description());
    ASSERT_TRUE(video != NULL);
    ASSERT_EQ(kVideoSsrc, video->first_ssrc());
  }

  void CheckDataSsrcForIncomingAccept(const cricket::Session* session) {
    const cricket::DataContentDescription* data =
        GetFirstDataContentDescription(session->remote_description());
    ASSERT_TRUE(data != NULL);
    ASSERT_EQ(kDataSsrc, data->first_ssrc());
  }

  void TestGoodIncomingInitiate(const std::string& initiate_string,
                                const cricket::CallOptions& options,
                                buzz::XmlElement** element) {
    *element = NULL;

    talk_base::scoped_ptr<buzz::XmlElement> el(
        buzz::XmlElement::ForStr(initiate_string));
    client_->session_manager()->OnIncomingMessage(el.get());
    ASSERT_TRUE(call_ != NULL);
    ASSERT_TRUE(call_->sessions()[0] != NULL);
    ASSERT_EQ(cricket::Session::STATE_RECEIVEDINITIATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_RESULT), stanzas_[0]->Attr(buzz::QN_TYPE));
    ClearStanzas();
    CheckVideoBandwidth(expected_video_bandwidth_,
                        call_->sessions()[0]->remote_description());
    CheckVideoRtcpMux(expected_video_rtcp_mux_,
                      call_->sessions()[0]->remote_description());
    if (expect_incoming_crypto_) {
      CheckCryptoFromGoodIncomingInitiate(call_->sessions()[0]);
    }

    // TODO(pthatcher): Add tests for sending <bandwidth> in accept.
    call_->AcceptSession(call_->sessions()[0], options);
    ASSERT_EQ(cricket::Session::STATE_SENTACCEPT,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));

    buzz::XmlElement* e = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(ContentFromAction(e) != NULL);
    *element = CopyElement(ContentFromAction(e));
    ASSERT_TRUE(*element != NULL);
    ClearStanzas();
    if (expect_outgoing_crypto_) {
      CheckCryptoForGoodOutgoingAccept(call_->sessions()[0]);
    }

  if (options.data_channel_type == cricket::DCT_RTP) {
      CheckDataRtcpMux(true, call_->sessions()[0]->local_description());
      CheckDataRtcpMux(true, call_->sessions()[0]->remote_description());
      // TODO(pthatcher): Check rtcpmux and crypto?
    }

    call_->Terminate();
    ASSERT_EQ(cricket::Session::STATE_SENTTERMINATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));
    e = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(parser_->ActionIsTerminate(e));
    ClearStanzas();
  }

  void TestRejectOffer(const std::string &initiate_string,
                       const cricket::CallOptions& options,
                       buzz::XmlElement** element) {
    *element = NULL;

    talk_base::scoped_ptr<buzz::XmlElement> el(
        buzz::XmlElement::ForStr(initiate_string));
    client_->session_manager()->OnIncomingMessage(el.get());
    ASSERT_TRUE(call_ != NULL);
    ASSERT_TRUE(call_->sessions()[0] != NULL);
    ASSERT_EQ(cricket::Session::STATE_RECEIVEDINITIATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_RESULT), stanzas_[0]->Attr(buzz::QN_TYPE));
    ClearStanzas();

    call_->AcceptSession(call_->sessions()[0], options);
    ASSERT_EQ(cricket::Session::STATE_SENTACCEPT,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));

    buzz::XmlElement* e = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(ContentFromAction(e) != NULL);
    *element = CopyElement(ContentFromAction(e));
    ASSERT_TRUE(*element != NULL);
    ClearStanzas();

    buzz::XmlElement* content = *element;
    // The NextContent method actually returns the second content. So we
    // can't handle the case when audio, video and data are all enabled. But
    // since we are testing rejection, it won't be the case.
    if (options.has_audio) {
      ASSERT_TRUE(content != NULL);
      ASSERT_EQ("test audio", content->Attr(buzz::QName("", "name")));
      content = parser_->NextContent(content);
    }

    if (options.has_video) {
      ASSERT_TRUE(content != NULL);
      ASSERT_EQ("test video", content->Attr(buzz::QName("", "name")));
      content = parser_->NextContent(content);
    }

    if (options.has_data()) {
      ASSERT_TRUE(content != NULL);
      ASSERT_EQ("test data", content->Attr(buzz::QName("", "name")));
      content = parser_->NextContent(content);
    }

    call_->Terminate();
    ASSERT_EQ(cricket::Session::STATE_SENTTERMINATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));
    e = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(parser_->ActionIsTerminate(e));
    ClearStanzas();
  }

  void TestBadIncomingInitiate(const std::string& initiate_string) {
    talk_base::scoped_ptr<buzz::XmlElement> el(
        buzz::XmlElement::ForStr(initiate_string));
    client_->session_manager()->OnIncomingMessage(el.get());
    ASSERT_TRUE(call_ != NULL);
    ASSERT_TRUE(call_->sessions()[0] != NULL);
    ASSERT_EQ(cricket::Session::STATE_SENTREJECT,
              call_->sessions()[0]->state());
    ASSERT_EQ(2U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[1]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_RESULT), stanzas_[1]->Attr(buzz::QN_TYPE));
    ClearStanzas();
  }

  void TestGoodOutgoingInitiate(const cricket::CallOptions& options) {
    client_->CreateCall();
    ASSERT_TRUE(call_ != NULL);
    call_->InitiateSession(buzz::Jid("me@mydomain.com"),
                           buzz::Jid("me@mydomain.com"), options);
    ASSERT_TRUE(call_->sessions()[0] != NULL);
    ASSERT_EQ(cricket::Session::STATE_SENTINITIATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));
    buzz::XmlElement* action = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(action != NULL);
    buzz::XmlElement* content = ContentFromAction(action);
    ASSERT_TRUE(content != NULL);

    buzz::XmlElement* e = PayloadTypeFromContent(content);
    ASSERT_TRUE(e != NULL);
    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(103, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(0, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(104, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(32000, codec.clockrate);
    ASSERT_EQ(0, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(119, codec.id);
    ASSERT_EQ("ISACLC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(40000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(99, codec.id);
    ASSERT_EQ("speex", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(22000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(97, codec.id);
    ASSERT_EQ("IPCMWB", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(80000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

     e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(9, codec.id);
    ASSERT_EQ("G722", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(102, codec.id);
    ASSERT_EQ("iLBC", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(13300, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(98, codec.id);
    ASSERT_EQ("speex", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(11000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(3, codec.id);
    ASSERT_EQ("GSM", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(13000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(100, codec.id);
    ASSERT_EQ("EG711U", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(101, codec.id);
    ASSERT_EQ("EG711A", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(0, codec.id);
    ASSERT_EQ("PCMU", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(8, codec.id);
    ASSERT_EQ("PCMA", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(126, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(32000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(105, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(13, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(117, codec.id);
    ASSERT_EQ("red", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(106, codec.id);
    ASSERT_EQ("telephone-event", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);

    if (expect_outgoing_crypto_) {
      buzz::XmlElement* encryption = EncryptionFromContent(content);
      ASSERT_TRUE(encryption != NULL);

      if (client_->secure() == cricket::SEC_REQUIRED) {
        ASSERT_TRUE(cricket::GetXmlAttr(
            encryption, cricket::QN_ENCRYPTION_REQUIRED, false));
      }

      if (content->Name().Namespace() == cricket::NS_GINGLE_AUDIO) {
        e = encryption->FirstNamed(cricket::QN_GINGLE_AUDIO_CRYPTO_USAGE);
        ASSERT_TRUE(e != NULL);
        ASSERT_TRUE(
            e->NextNamed(cricket::QN_GINGLE_AUDIO_CRYPTO_USAGE) == NULL);
        ASSERT_TRUE(
            e->FirstNamed(cricket::QN_GINGLE_VIDEO_CRYPTO_USAGE) == NULL);
      }

      e = encryption->FirstNamed(cricket::QN_CRYPTO);
      ASSERT_TRUE(e != NULL);
      ASSERT_EQ("0", e->Attr(cricket::QN_CRYPTO_TAG));
      ASSERT_EQ("AES_CM_128_HMAC_SHA1_32", e->Attr(cricket::QN_CRYPTO_SUITE));
      std::string key_0 = e->Attr(cricket::QN_CRYPTO_KEY_PARAMS);
      ASSERT_EQ(47U, key_0.length());
      ASSERT_EQ("inline:", key_0.substr(0, 7));

      e = e->NextNamed(cricket::QN_CRYPTO);
      ASSERT_TRUE(e != NULL);
      ASSERT_EQ("1", e->Attr(cricket::QN_CRYPTO_TAG));
      ASSERT_EQ("AES_CM_128_HMAC_SHA1_80", e->Attr(cricket::QN_CRYPTO_SUITE));
      std::string key_1 = e->Attr(cricket::QN_CRYPTO_KEY_PARAMS);
      ASSERT_EQ(47U, key_1.length());
      ASSERT_EQ("inline:", key_1.substr(0, 7));
      ASSERT_NE(key_0, key_1);

      encryption = NextFromEncryption(encryption);
      ASSERT_TRUE(encryption == NULL);
    }

    if (options.has_video) {
      CheckVideoBandwidth(options.video_bandwidth,
                          call_->sessions()[0]->local_description());
      CheckVideoRtcpMux(expected_video_rtcp_mux_,
                        call_->sessions()[0]->remote_description());
      content = parser_->NextContent(content);
      const buzz::XmlElement* bandwidth =
          parser_->BandwidthFromContent(content);
      if (options.video_bandwidth == cricket::kAutoBandwidth) {
        ASSERT_TRUE(bandwidth == NULL);
      } else {
        ASSERT_TRUE(bandwidth != NULL);
        ASSERT_EQ("AS", bandwidth->Attr(buzz::QName("", "type")));
        ASSERT_EQ(talk_base::ToString(options.video_bandwidth / 1000),
                  bandwidth->BodyText());
      }
    }

    if (options.data_channel_type == cricket::DCT_RTP) {
      content = parser_->NextContent(content);
      CheckRtpDataContent(content);
    }

    if (options.data_channel_type == cricket::DCT_SCTP) {
      content = parser_->NextContent(content);
      CheckSctpDataContent(content);
    }

    ClearStanzas();
  }

  void TestHasAllSupportedAudioCodecs(buzz::XmlElement* e) {
    ASSERT_TRUE(e != NULL);

    e = PayloadTypeFromContent(e);
    ASSERT_TRUE(e != NULL);
    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(103, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(104, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(32000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(119, codec.id);
    ASSERT_EQ("ISACLC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(40000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(99, codec.id);
    ASSERT_EQ("speex", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(22000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(97, codec.id);
    ASSERT_EQ("IPCMWB", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(80000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(9, codec.id);
    ASSERT_EQ("G722", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(102, codec.id);
    ASSERT_EQ("iLBC", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(13300, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(98, codec.id);
    ASSERT_EQ("speex", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(11000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(3, codec.id);
    ASSERT_EQ("GSM", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(13000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(100, codec.id);
    ASSERT_EQ("EG711U", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(101, codec.id);
    ASSERT_EQ("EG711A", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(0, codec.id);
    ASSERT_EQ("PCMU", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(8, codec.id);
    ASSERT_EQ("PCMA", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(64000, codec.bitrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(126, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(32000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(105, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(13, codec.id);
    ASSERT_EQ("CN", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(117, codec.id);
    ASSERT_EQ("red", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(106, codec.id);
    ASSERT_EQ("telephone-event", codec.name);
    ASSERT_EQ(8000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);
  }

  void TestCodecsOfVideoInitiate(buzz::XmlElement* content) {
    ASSERT_TRUE(content != NULL);
    buzz::XmlElement* payload_type = PayloadTypeFromContent(content);
    ASSERT_TRUE(payload_type != NULL);
    cricket::AudioCodec codec = AudioCodecFromPayloadType(payload_type);
    ASSERT_EQ(103, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    content = parser_->NextContent(content);
    ASSERT_TRUE(content != NULL);
    payload_type = PayloadTypeFromContent(content);
    ASSERT_TRUE(payload_type != NULL);
    cricket::VideoCodec vcodec =
        parser_->VideoCodecFromPayloadType(payload_type);
    ASSERT_EQ(99, vcodec.id);
    ASSERT_EQ("H264-SVC", vcodec.name);
    ASSERT_EQ(320, vcodec.width);
    ASSERT_EQ(200, vcodec.height);
    ASSERT_EQ(30, vcodec.framerate);
  }

  void TestHasAudioCodecsFromInitiateSomeUnsupported(buzz::XmlElement* e) {
    ASSERT_TRUE(e != NULL);
    e = PayloadTypeFromContent(e);
    ASSERT_TRUE(e != NULL);

    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(103, codec.id);
    ASSERT_EQ("ISAC", codec.name);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(100, codec.id);
    ASSERT_EQ("EG711U", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(101, codec.id);
    ASSERT_EQ("EG711A", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(0, codec.id);
    ASSERT_EQ("PCMU", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(13, codec.id);
    ASSERT_EQ("CN", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);
  }

  void TestHasAudioCodecsFromInitiateDynamicAudioCodecs(
      buzz::XmlElement* e) {
    ASSERT_TRUE(e != NULL);
    e = PayloadTypeFromContent(e);
    ASSERT_TRUE(e != NULL);

    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(123, codec.id);
    ASSERT_EQ(16000, codec.clockrate);
    ASSERT_EQ(1, codec.channels);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);
  }

  void TestHasDefaultAudioCodecs(buzz::XmlElement* e) {
    ASSERT_TRUE(e != NULL);
    e = PayloadTypeFromContent(e);
    ASSERT_TRUE(e != NULL);

    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(103, codec.id);
    ASSERT_EQ("ISAC", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(0, codec.id);
    ASSERT_EQ("PCMU", codec.name);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);
  }

  void TestHasAudioCodecsFromInitiateStaticAudioCodecs(
      buzz::XmlElement* e) {
    ASSERT_TRUE(e != NULL);
    e = PayloadTypeFromContent(e);
    ASSERT_TRUE(e != NULL);

    cricket::AudioCodec codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(3, codec.id);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(0, codec.id);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e != NULL);
    codec = AudioCodecFromPayloadType(e);
    ASSERT_EQ(8, codec.id);

    e = NextFromPayloadType(e);
    ASSERT_TRUE(e == NULL);
  }

  void TestGingleInitiateWithUnsupportedCrypto(
      const std::string &initiate_string,
      buzz::XmlElement** element) {
    *element = NULL;

    talk_base::scoped_ptr<buzz::XmlElement> el(
        buzz::XmlElement::ForStr(initiate_string));
    client_->session_manager()->OnIncomingMessage(el.get());

    ASSERT_EQ(cricket::Session::STATE_RECEIVEDINITIATE,
              call_->sessions()[0]->state());
    ClearStanzas();
    CheckBadCryptoFromIncomingInitiate(call_->sessions()[0]);

    call_->AcceptSession(call_->sessions()[0], cricket::CallOptions());
    ClearStanzas();
    CheckNoCryptoForOutgoingAccept(call_->sessions()[0]);

    call_->Terminate();
    ASSERT_EQ(cricket::Session::STATE_SENTTERMINATE,
              call_->sessions()[0]->state());
    ClearStanzas();
  }

  void TestIncomingAcceptWithSsrcs(
      const std::string& accept_string,
      cricket::CallOptions& options) {
    client_->CreateCall();
    ASSERT_TRUE(call_ != NULL);

    call_->InitiateSession(buzz::Jid("me@mydomain.com"),
                           buzz::Jid("me@mydomain.com"), options);
    ASSERT_TRUE(call_->sessions()[0] != NULL);
    ASSERT_EQ(cricket::Session::STATE_SENTINITIATE,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_SET), stanzas_[0]->Attr(buzz::QN_TYPE));
    buzz::XmlElement* action = ActionFromStanza(stanzas_[0]);
    ASSERT_TRUE(action != NULL);
    buzz::XmlElement* content = ContentFromAction(action);
    ASSERT_TRUE(content != NULL);
    if (initial_protocol_ == cricket::PROTOCOL_JINGLE) {
      buzz::XmlElement* content_desc =
          content->FirstNamed(cricket::QN_JINGLE_RTP_CONTENT);
      ASSERT_TRUE(content_desc != NULL);
      ASSERT_EQ("", content_desc->Attr(cricket::QN_SSRC));
    }
    ClearStanzas();

    // We need to insert the session ID into the session accept message.
    talk_base::scoped_ptr<buzz::XmlElement> el(
        buzz::XmlElement::ForStr(accept_string));
    const std::string sid = call_->sessions()[0]->id();
    if (initial_protocol_ == cricket::PROTOCOL_JINGLE) {
      buzz::XmlElement* jingle = el->FirstNamed(cricket::QN_JINGLE);
      jingle->SetAttr(cricket::QN_SID, sid);
    } else {
      buzz::XmlElement* session = el->FirstNamed(cricket::QN_GINGLE_SESSION);
      session->SetAttr(cricket::QN_ID, sid);
    }

    client_->session_manager()->OnIncomingMessage(el.get());

    ASSERT_EQ(cricket::Session::STATE_RECEIVEDACCEPT,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_TRUE(buzz::QN_IQ == stanzas_[0]->Name());
    ASSERT_TRUE(stanzas_[0]->HasAttr(buzz::QN_TYPE));
    ASSERT_EQ(std::string(buzz::STR_RESULT), stanzas_[0]->Attr(buzz::QN_TYPE));
    ClearStanzas();

    CheckAudioSsrcForIncomingAccept(call_->sessions()[0]);
    CheckVideoSsrcForIncomingAccept(call_->sessions()[0]);
    if (options.data_channel_type == cricket::DCT_RTP) {
      CheckDataSsrcForIncomingAccept(call_->sessions()[0]);
    }
    // TODO(pthatcher): Check kDataSid if DCT_SCTP.
  }

  size_t ClearStanzas() {
    size_t size = stanzas_.size();
    for (size_t i = 0; i < size; i++) {
      delete stanzas_[i];
    }
    stanzas_.clear();
    return size;
  }

  buzz::XmlElement* SetJingleSid(buzz::XmlElement* stanza) {
    buzz::XmlElement* jingle =
        stanza->FirstNamed(cricket::QN_JINGLE);
    jingle->SetAttr(cricket::QN_SID, call_->sessions()[0]->id());
    return stanza;
  }

  void TestSendVideoStreamUpdate() {
    cricket::CallOptions options = VideoCallOptions();
    options.is_muc = true;

    client_->CreateCall();
    call_->InitiateSession(buzz::Jid("me@mydomain.com"),
                           buzz::Jid("me@mydomain.com"), options);
    ClearStanzas();

    cricket::StreamParams stream;
    stream.id = "test-stream";
    stream.ssrcs.push_back(1001);
    talk_base::scoped_ptr<buzz::XmlElement> expected_stream_add(
        buzz::XmlElement::ForStr(
            JingleOutboundStreamAdd(
                call_->sessions()[0]->id(),
                "video", stream.id, "1001")));
    talk_base::scoped_ptr<buzz::XmlElement> expected_stream_remove(
        buzz::XmlElement::ForStr(
            JingleOutboundStreamRemove(
                call_->sessions()[0]->id(),
                "video", stream.id)));

    call_->SendVideoStreamUpdate(call_->sessions()[0],
                                 call_->CreateVideoStreamUpdate(stream));
    ASSERT_EQ(1U, stanzas_.size());
    EXPECT_EQ(expected_stream_add->Str(), stanzas_[0]->Str());
    ClearStanzas();

    stream.ssrcs.clear();
    call_->SendVideoStreamUpdate(call_->sessions()[0],
                                 call_->CreateVideoStreamUpdate(stream));
    ASSERT_EQ(1U, stanzas_.size());
    EXPECT_EQ(expected_stream_remove->Str(), stanzas_[0]->Str());
    ClearStanzas();
  }

  void TestStreamsUpdateAndViewRequests() {
    cricket::CallOptions options = VideoCallOptions();
    options.is_muc = true;

    client_->CreateCall();
    call_->InitiateSession(buzz::Jid("me@mydomain.com"),
                           buzz::Jid("me@mydomain.com"), options);
    ASSERT_EQ(1U, ClearStanzas());
    ASSERT_EQ(0U, last_streams_added_.audio().size());
    ASSERT_EQ(0U, last_streams_added_.video().size());
    ASSERT_EQ(0U, last_streams_removed_.audio().size());
    ASSERT_EQ(0U, last_streams_removed_.video().size());

    talk_base::scoped_ptr<buzz::XmlElement> accept_stanza(
        buzz::XmlElement::ForStr(kJingleAcceptWithSsrcs));
    SetJingleSid(accept_stanza.get());
    client_->session_manager()->OnIncomingMessage(accept_stanza.get());
    ASSERT_EQ(cricket::Session::STATE_RECEIVEDACCEPT,
              call_->sessions()[0]->state());
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_EQ(std::string(buzz::STR_RESULT), stanzas_[0]->Attr(buzz::QN_TYPE));
    ClearStanzas();
    // Need to clear the added streams, because they are populated when
    // receiving an accept message now.
    last_streams_added_.mutable_video()->clear();
    last_streams_added_.mutable_audio()->clear();

    call_->sessions()[0]->SetState(cricket::Session::STATE_INPROGRESS);

    talk_base::scoped_ptr<buzz::XmlElement> streams_stanza(
        buzz::XmlElement::ForStr(
            JingleStreamAdd("video", "Bob", "video1", "ABC")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    // First one is ignored because of bad syntax.
    ASSERT_EQ(1U, stanzas_.size());
    // TODO(pthatcher): Figure out how to make this an ERROR rather than RESULT.
    ASSERT_EQ(std::string(buzz::STR_ERROR), stanzas_[0]->Attr(buzz::QN_TYPE));
    ClearStanzas();
    ASSERT_EQ(0U, last_streams_added_.audio().size());
    ASSERT_EQ(0U, last_streams_added_.video().size());
    ASSERT_EQ(0U, last_streams_removed_.audio().size());
    ASSERT_EQ(0U, last_streams_removed_.video().size());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAdd("audio", "Bob", "audio1", "1234")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_added_.audio().size());
    ASSERT_EQ("Bob", last_streams_added_.audio()[0].groupid);
    ASSERT_EQ(1U, last_streams_added_.audio()[0].ssrcs.size());
    ASSERT_EQ(1234U, last_streams_added_.audio()[0].first_ssrc());

    // Ignores adds without ssrcs.
    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAddWithoutSsrc("audio", "Bob", "audioX")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_added_.audio().size());
    ASSERT_EQ(1234U, last_streams_added_.audio()[0].first_ssrc());

    // Ignores stream updates with unknown content names. (Don't terminate).
    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAddWithoutSsrc("foo", "Bob", "foo")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAdd("audio", "Joe", "audio1", "2468")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_added_.audio().size());
    ASSERT_EQ("Joe", last_streams_added_.audio()[0].groupid);
    ASSERT_EQ(1U, last_streams_added_.audio()[0].ssrcs.size());
    ASSERT_EQ(2468U, last_streams_added_.audio()[0].first_ssrc());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAdd("video", "Bob", "video1", "5678")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_added_.video().size());
    ASSERT_EQ("Bob", last_streams_added_.video()[0].groupid);
    ASSERT_EQ(1U, last_streams_added_.video()[0].ssrcs.size());
    ASSERT_EQ(5678U, last_streams_added_.video()[0].first_ssrc());

    // We're testing that a "duplicate" is effectively ignored.
    last_streams_added_.mutable_video()->clear();
    last_streams_removed_.mutable_video()->clear();
    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAdd("video", "Bob", "video1", "5678")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(0U, last_streams_added_.video().size());
    ASSERT_EQ(0U, last_streams_removed_.video().size());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamAdd("video", "Bob", "video2", "5679")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_added_.video().size());
    ASSERT_EQ("Bob", last_streams_added_.video()[0].groupid);
    ASSERT_EQ(1U, last_streams_added_.video()[0].ssrcs.size());
    ASSERT_EQ(5679U, last_streams_added_.video()[0].first_ssrc());

    cricket::FakeVoiceMediaChannel* voice_channel = fme_->GetVoiceChannel(0);
    ASSERT_TRUE(voice_channel != NULL);
    ASSERT_TRUE(voice_channel->HasRecvStream(1234U));
    ASSERT_TRUE(voice_channel->HasRecvStream(2468U));
    cricket::FakeVideoMediaChannel* video_channel = fme_->GetVideoChannel(0);
    ASSERT_TRUE(video_channel != NULL);
    ASSERT_TRUE(video_channel->HasRecvStream(5678U));
    ClearStanzas();

    cricket::ViewRequest viewRequest;
    cricket::StaticVideoView staticVideoView(
        cricket::StreamSelector(5678U), 640, 480, 30);
    viewRequest.static_video_views.push_back(staticVideoView);
    talk_base::scoped_ptr<buzz::XmlElement> expected_view_elem(
        buzz::XmlElement::ForStr(JingleView("5678", "640", "480", "30")));
    SetJingleSid(expected_view_elem.get());

    ASSERT_TRUE(
        call_->SendViewRequest(call_->sessions()[0], viewRequest));
    ASSERT_EQ(1U, stanzas_.size());
    ASSERT_EQ(expected_view_elem->Str(), stanzas_[0]->Str());
    ClearStanzas();

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamRemove("audio", "Bob", "audio1")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_removed_.audio().size());
    ASSERT_EQ(1U, last_streams_removed_.audio()[0].ssrcs.size());
    EXPECT_EQ(1234U, last_streams_removed_.audio()[0].first_ssrc());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamRemove("video", "Bob", "video1")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_removed_.video().size());
    ASSERT_EQ(1U, last_streams_removed_.video()[0].ssrcs.size());
    EXPECT_EQ(5678U, last_streams_removed_.video()[0].first_ssrc());

    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamRemove("video", "Bob", "video2")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(1U, last_streams_removed_.video().size());
    ASSERT_EQ(1U, last_streams_removed_.video()[0].ssrcs.size());
    EXPECT_EQ(5679U, last_streams_removed_.video()[0].first_ssrc());

    // Duplicate removal: should be ignored.
    last_streams_removed_.mutable_audio()->clear();
    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamRemove("audio", "Bob", "audio1")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(0U, last_streams_removed_.audio().size());

    // Duplicate removal: should be ignored.
    last_streams_removed_.mutable_video()->clear();
    streams_stanza.reset(buzz::XmlElement::ForStr(
        JingleStreamRemove("video", "Bob", "video1")));
    SetJingleSid(streams_stanza.get());
    client_->session_manager()->OnIncomingMessage(streams_stanza.get());
    ASSERT_EQ(0U, last_streams_removed_.video().size());

    voice_channel = fme_->GetVoiceChannel(0);
    ASSERT_TRUE(voice_channel != NULL);
    ASSERT_FALSE(voice_channel->HasRecvStream(1234U));
    ASSERT_TRUE(voice_channel->HasRecvStream(2468U));
    video_channel = fme_->GetVideoChannel(0);
    ASSERT_TRUE(video_channel != NULL);
    ASSERT_FALSE(video_channel->HasRecvStream(5678U));

    // Fails because ssrc is now invalid.
    ASSERT_FALSE(
        call_->SendViewRequest(call_->sessions()[0], viewRequest));

    ClearStanzas();
  }

  void MakeSignalingSecure(cricket::SecureMediaPolicy secure) {
    client_->set_secure(secure);
  }

  void ExpectCrypto(cricket::SecureMediaPolicy secure) {
    MakeSignalingSecure(secure);
    expect_incoming_crypto_ = true;
#ifdef HAVE_SRTP
    expect_outgoing_crypto_ = true;
#endif
  }

  void ExpectVideoBandwidth(int bandwidth) {
    expected_video_bandwidth_ = bandwidth;
  }

  void ExpectVideoRtcpMux(bool rtcp_mux) {
    expected_video_rtcp_mux_ = rtcp_mux;
  }

 private:
  void OnSendStanza(cricket::SessionManager* manager,
                    const buzz::XmlElement* stanza) {
    LOG(LS_INFO) << stanza->Str();
    stanzas_.push_back(new buzz::XmlElement(*stanza));
  }

  void OnSessionCreate(cricket::Session* session, bool initiate) {
    session->set_current_protocol(initial_protocol_);
  }

  void OnCallCreate(cricket::Call *call) {
    call_ = call;
    call->SignalMediaStreamsUpdate.connect(
        this, &MediaSessionClientTest::OnMediaStreamsUpdate);
  }

  void OnCallDestroy(cricket::Call *call) {
    call_ = NULL;
  }

  void OnMediaStreamsUpdate(cricket::Call *call,
                            cricket::Session *session,
                            const cricket::MediaStreams& added,
                            const cricket::MediaStreams& removed) {
    last_streams_added_.CopyFrom(added);
    last_streams_removed_.CopyFrom(removed);
  }

  talk_base::NetworkManager* nm_;
  cricket::PortAllocator* pa_;
  cricket::SessionManager* sm_;
  cricket::FakeMediaEngine* fme_;
  cricket::FakeDataEngine* fdme_;
  cricket::MediaSessionClient* client_;

  cricket::Call* call_;
  std::vector<buzz::XmlElement* > stanzas_;
  MediaSessionTestParser* parser_;
  cricket::SignalingProtocol initial_protocol_;
  bool expect_incoming_crypto_;
  bool expect_outgoing_crypto_;
  int expected_video_bandwidth_;
  bool expected_video_rtcp_mux_;
  cricket::MediaStreams last_streams_added_;
  cricket::MediaStreams last_streams_removed_;
};

MediaSessionClientTest* GingleTest() {
  return new MediaSessionClientTest(new GingleSessionTestParser(),
                                    cricket::PROTOCOL_GINGLE);
}

MediaSessionClientTest* JingleTest() {
  return new MediaSessionClientTest(new JingleSessionTestParser(),
                                    cricket::PROTOCOL_JINGLE);
}

TEST(MediaSessionTest, JingleGoodVideoInitiate) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleVideoInitiate, VideoCallOptions(), elem.use());
  test->TestCodecsOfVideoInitiate(elem.get());
}

TEST(MediaSessionTest, JingleGoodVideoInitiateWithBandwidth) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->ExpectVideoBandwidth(42000);
  test->TestGoodIncomingInitiate(
      kJingleVideoInitiateWithBandwidth, VideoCallOptions(), elem.use());
}

TEST(MediaSessionTest, JingleGoodVideoInitiateWithRtcpMux) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->ExpectVideoRtcpMux(true);
  test->TestGoodIncomingInitiate(
      kJingleVideoInitiateWithRtcpMux, VideoCallOptions(), elem.use());
}

TEST(MediaSessionTest, JingleGoodVideoInitiateWithRtpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  cricket::CallOptions options = VideoCallOptions();
  options.data_channel_type = cricket::DCT_RTP;
  test->TestGoodIncomingInitiate(
      AddEncryption(kJingleVideoInitiateWithRtpData, kJingleCryptoOffer),
      options,
      elem.use());
}

TEST(MediaSessionTest, JingleGoodVideoInitiateWithSctpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  cricket::CallOptions options = VideoCallOptions();
  options.data_channel_type = cricket::DCT_SCTP;
  test->TestGoodIncomingInitiate(kJingleVideoInitiateWithSctpData,
                                 options,
                                 elem.use());
}

TEST(MediaSessionTest, JingleRejectAudio) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  cricket::CallOptions options = VideoCallOptions();
  options.has_audio = false;
  options.data_channel_type = cricket::DCT_RTP;
  test->TestRejectOffer(kJingleVideoInitiateWithRtpData, options, elem.use());
}

TEST(MediaSessionTest, JingleRejectVideo) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  cricket::CallOptions options = AudioCallOptions();
  options.data_channel_type = cricket::DCT_RTP;
  test->TestRejectOffer(kJingleVideoInitiateWithRtpData, options, elem.use());
}

TEST(MediaSessionTest, JingleRejectData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestRejectOffer(
      kJingleVideoInitiateWithRtpData, VideoCallOptions(), elem.use());
}

TEST(MediaSessionTest, JingleRejectVideoAndData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestRejectOffer(
      kJingleVideoInitiateWithRtpData, AudioCallOptions(), elem.use());
}

TEST(MediaSessionTest, JingleGoodInitiateAllSupportedAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleInitiate, AudioCallOptions(), elem.use());
  test->TestHasAllSupportedAudioCodecs(elem.get());
}

TEST(MediaSessionTest, JingleGoodInitiateDifferentPreferenceAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleInitiateDifferentPreference, AudioCallOptions(), elem.use());
  test->TestHasAllSupportedAudioCodecs(elem.get());
}

TEST(MediaSessionTest, JingleGoodInitiateSomeUnsupportedAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleInitiateSomeUnsupported, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateSomeUnsupported(elem.get());
}

TEST(MediaSessionTest, JingleGoodInitiateDynamicAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleInitiateDynamicAudioCodecs, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateDynamicAudioCodecs(elem.get());
}

TEST(MediaSessionTest, JingleGoodInitiateStaticAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      kJingleInitiateStaticAudioCodecs, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateStaticAudioCodecs(elem.get());
}

TEST(MediaSessionTest, JingleBadInitiateNoAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateNoAudioCodecs);
}

TEST(MediaSessionTest, JingleBadInitiateNoSupportedAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateNoSupportedAudioCodecs);
}

TEST(MediaSessionTest, JingleBadInitiateWrongClockrates) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateWrongClockrates);
}

TEST(MediaSessionTest, JingleBadInitiateWrongChannels) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateWrongChannels);
}

TEST(MediaSessionTest, JingleBadInitiateNoPayloadTypes) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateNoPayloadTypes);
}

TEST(MediaSessionTest, JingleBadInitiateDynamicWithoutNames) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(kJingleInitiateDynamicWithoutNames);
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiate) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithBandwidth) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.video_bandwidth = 42000;
  test->TestGoodOutgoingInitiate(options);
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithRtcpMux) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.rtcp_mux_enabled = true;
  test->TestGoodOutgoingInitiate(options);
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithRtpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options;
  options.data_channel_type = cricket::DCT_RTP;
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodOutgoingInitiate(options);
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithSctpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options;
  options.data_channel_type = cricket::DCT_SCTP;
  test->TestGoodOutgoingInitiate(options);
}

// Crypto related tests.

// Offer has crypto but the session is not secured, just ignore it.
TEST(MediaSessionTest, JingleInitiateWithCryptoIsIgnoredWhenNotSecured) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->TestGoodIncomingInitiate(
      AddEncryption(kJingleVideoInitiate, kJingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has crypto required but the session is not secure, fail.
TEST(MediaSessionTest, JingleInitiateWithCryptoRequiredWhenNotSecured) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(AddEncryption(kJingleVideoInitiate,
                                             kJingleRequiredCryptoOffer));
}

// Offer has no crypto but the session is secure required, fail.
TEST(MediaSessionTest, JingleInitiateWithNoCryptoFailsWhenSecureRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestBadIncomingInitiate(kJingleInitiate);
}

// Offer has crypto and session is secure, expect crypto in the answer.
TEST(MediaSessionTest, JingleInitiateWithCryptoWhenSecureEnabled) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kJingleVideoInitiate, kJingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has crypto and session is secure required, expect crypto in
// the answer.
TEST(MediaSessionTest, JingleInitiateWithCryptoWhenSecureRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kJingleVideoInitiate, kJingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has unsupported crypto and session is secure, no crypto in
// the answer.
TEST(MediaSessionTest, JingleInitiateWithUnsupportedCrypto) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  test->MakeSignalingSecure(cricket::SEC_ENABLED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kJingleInitiate, kJingleUnsupportedCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has unsupported REQUIRED crypto and session is not secure, fail.
TEST(MediaSessionTest, JingleInitiateWithRequiredUnsupportedCrypto) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestBadIncomingInitiate(
      AddEncryption(kJingleInitiate, kJingleRequiredUnsupportedCryptoOffer));
}

// Offer has unsupported REQUIRED crypto and session is secure, fail.
TEST(MediaSessionTest, JingleInitiateWithRequiredUnsupportedCryptoWhenSecure) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->MakeSignalingSecure(cricket::SEC_ENABLED);
  test->TestBadIncomingInitiate(
      AddEncryption(kJingleInitiate, kJingleRequiredUnsupportedCryptoOffer));
}

// Offer has unsupported REQUIRED crypto and session is required secure, fail.
TEST(MediaSessionTest,
     JingleInitiateWithRequiredUnsupportedCryptoWhenSecureRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->MakeSignalingSecure(cricket::SEC_REQUIRED);
  test->TestBadIncomingInitiate(
      AddEncryption(kJingleInitiate, kJingleRequiredUnsupportedCryptoOffer));
}


TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithCrypto) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, JingleGoodOutgoingInitiateWithCryptoRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, JingleIncomingAcceptWithSsrcs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.is_muc = true;
  test->TestIncomingAcceptWithSsrcs(kJingleAcceptWithSsrcs, options);
}

TEST(MediaSessionTest, JingleIncomingAcceptWithRtpDataSsrcs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.is_muc = true;
  options.data_channel_type = cricket::DCT_RTP;
  test->TestIncomingAcceptWithSsrcs(kJingleAcceptWithRtpDataSsrcs, options);
}

TEST(MediaSessionTest, JingleIncomingAcceptWithSctpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.is_muc = true;
  options.data_channel_type = cricket::DCT_SCTP;
  test->TestIncomingAcceptWithSsrcs(kJingleAcceptWithSctpData, options);
}

TEST(MediaSessionTest, JingleStreamsUpdateAndView) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestStreamsUpdateAndViewRequests();
}

TEST(MediaSessionTest, JingleSendVideoStreamUpdate) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(JingleTest());
  test->TestSendVideoStreamUpdate();
}

// Gingle tests

TEST(MediaSessionTest, GingleGoodVideoInitiate) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleVideoInitiate, VideoCallOptions(), elem.use());
  test->TestCodecsOfVideoInitiate(elem.get());
}

TEST(MediaSessionTest, GingleGoodVideoInitiateWithBandwidth) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectVideoBandwidth(42000);
  test->TestGoodIncomingInitiate(
      kGingleVideoInitiateWithBandwidth, VideoCallOptions(), elem.use());
}

TEST(MediaSessionTest, GingleGoodInitiateAllSupportedAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiate, AudioCallOptions(), elem.use());
  test->TestHasAllSupportedAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateAllSupportedAudioCodecsWithCrypto) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleCryptoOffer),
      AudioCallOptions(),
      elem.use());
  test->TestHasAllSupportedAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateDifferentPreferenceAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiateDifferentPreference, AudioCallOptions(), elem.use());
  test->TestHasAllSupportedAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateSomeUnsupportedAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiateSomeUnsupported, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateSomeUnsupported(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateDynamicAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiateDynamicAudioCodecs, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateDynamicAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateStaticAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiateStaticAudioCodecs, AudioCallOptions(), elem.use());
  test->TestHasAudioCodecsFromInitiateStaticAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleGoodInitiateNoAudioCodecs) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      kGingleInitiateNoAudioCodecs, AudioCallOptions(), elem.use());
  test->TestHasDefaultAudioCodecs(elem.get());
}

TEST(MediaSessionTest, GingleBadInitiateNoSupportedAudioCodecs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(kGingleInitiateNoSupportedAudioCodecs);
}

TEST(MediaSessionTest, GingleBadInitiateWrongClockrates) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(kGingleInitiateWrongClockrates);
}

TEST(MediaSessionTest, GingleBadInitiateWrongChannels) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(kGingleInitiateWrongChannels);
}


TEST(MediaSessionTest, GingleBadInitiateNoPayloadTypes) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(kGingleInitiateNoPayloadTypes);
}

TEST(MediaSessionTest, GingleBadInitiateDynamicWithoutNames) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(kGingleInitiateDynamicWithoutNames);
}

TEST(MediaSessionTest, GingleGoodOutgoingInitiate) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, GingleGoodOutgoingInitiateWithBandwidth) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.video_bandwidth = 42000;
  test->TestGoodOutgoingInitiate(options);
}

// Crypto related tests.

// Offer has crypto but the session is not secured, just ignore it.
TEST(MediaSessionTest, GingleInitiateWithCryptoIsIgnoredWhenNotSecured) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestGoodIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has crypto required but the session is not secure, fail.
TEST(MediaSessionTest, GingleInitiateWithCryptoRequiredWhenNotSecured) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(AddEncryption(kGingleInitiate,
                                             kGingleRequiredCryptoOffer));
}

// Offer has no crypto but the session is secure required, fail.
TEST(MediaSessionTest, GingleInitiateWithNoCryptoFailsWhenSecureRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestBadIncomingInitiate(kGingleInitiate);
}

// Offer has crypto and session is secure, expect crypto in the answer.
TEST(MediaSessionTest, GingleInitiateWithCryptoWhenSecureEnabled) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has crypto and session is secure required, expect crypto in
// the answer.
TEST(MediaSessionTest, GingleInitiateWithCryptoWhenSecureRequired) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has unsupported crypto and session is secure, no crypto in
// the answer.
TEST(MediaSessionTest, GingleInitiateWithUnsupportedCrypto) {
  talk_base::scoped_ptr<buzz::XmlElement> elem;
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->MakeSignalingSecure(cricket::SEC_ENABLED);
  test->TestGoodIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleUnsupportedCryptoOffer),
      VideoCallOptions(),
      elem.use());
}

// Offer has unsupported REQUIRED crypto and session is not secure, fail.
TEST(MediaSessionTest, GingleInitiateWithRequiredUnsupportedCrypto) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->TestBadIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleRequiredUnsupportedCryptoOffer));
}

// Offer has unsupported REQUIRED crypto and session is secure, fail.
TEST(MediaSessionTest, GingleInitiateWithRequiredUnsupportedCryptoWhenSecure) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->MakeSignalingSecure(cricket::SEC_ENABLED);
  test->TestBadIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleRequiredUnsupportedCryptoOffer));
}

// Offer has unsupported REQUIRED crypto and session is required secure, fail.
TEST(MediaSessionTest,
     GingleInitiateWithRequiredUnsupportedCryptoWhenSecureRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->MakeSignalingSecure(cricket::SEC_REQUIRED);
  test->TestBadIncomingInitiate(
      AddEncryption(kGingleInitiate, kGingleRequiredUnsupportedCryptoOffer));
}

TEST(MediaSessionTest, GingleGoodOutgoingInitiateWithCrypto) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, GingleGoodOutgoingInitiateWithCryptoRequired) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  test->ExpectCrypto(cricket::SEC_REQUIRED);
  test->TestGoodOutgoingInitiate(AudioCallOptions());
}

TEST(MediaSessionTest, GingleIncomingAcceptWithSsrcs) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  cricket::CallOptions options = VideoCallOptions();
  options.is_muc = true;
  test->TestIncomingAcceptWithSsrcs(kGingleAcceptWithSsrcs, options);
}

TEST(MediaSessionTest, GingleGoodOutgoingInitiateWithRtpData) {
  talk_base::scoped_ptr<MediaSessionClientTest> test(GingleTest());
  cricket::CallOptions options;
  options.data_channel_type = cricket::DCT_RTP;
  test->ExpectCrypto(cricket::SEC_ENABLED);
  test->TestGoodOutgoingInitiate(options);
}
