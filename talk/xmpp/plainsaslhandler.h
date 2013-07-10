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

#ifndef _PLAINSASLHANDLER_H_
#define _PLAINSASLHANDLER_H_

#include "talk/xmpp/saslhandler.h"
#include <algorithm>

namespace buzz {

class PlainSaslHandler : public SaslHandler {
public:
  PlainSaslHandler(const Jid & jid, const talk_base::CryptString & password, 
      bool allow_plain) : jid_(jid), password_(password), 
                          allow_plain_(allow_plain) {}
    
  virtual ~PlainSaslHandler() {}

  // Should pick the best method according to this handler
  // returns the empty string if none are suitable
  virtual std::string ChooseBestSaslMechanism(const std::vector<std::string> & mechanisms, bool encrypted) {
  
    if (!encrypted && !allow_plain_) {
      return "";
    }
    
    std::vector<std::string>::const_iterator it = std::find(mechanisms.begin(), mechanisms.end(), "PLAIN");
    if (it == mechanisms.end()) {
      return "";
    }
    else {
      return "PLAIN";
    }
  }

  // Creates a SaslMechanism for the given mechanism name (you own it
  // once you get it).  If not handled, return NULL.
  virtual SaslMechanism * CreateSaslMechanism(const std::string & mechanism) {
    if (mechanism == "PLAIN") {
      return new SaslPlainMechanism(jid_, password_);
    }
    return NULL;
  }
  
private:
  Jid jid_;
  talk_base::CryptString password_;
  bool allow_plain_;
};


}

#endif

