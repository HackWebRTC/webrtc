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

#ifndef TALK_XMPP_JID_H_
#define TALK_XMPP_JID_H_

#include <string>
#include "talk/base/basictypes.h"
#include "talk/xmllite/xmlconstants.h"

namespace buzz {

// The Jid class encapsulates and provides parsing help for Jids. A Jid
// consists of three parts: the node, the domain and the resource, e.g.:
//
// node@domain/resource
//
// The node and resource are both optional. A valid jid is defined to have
// a domain. A bare jid is defined to not have a resource and a full jid
// *does* have a resource.
class Jid {
public:
  explicit Jid();
  explicit Jid(const std::string& jid_string);
  explicit Jid(const std::string& node_name,
               const std::string& domain_name,
               const std::string& resource_name);
  ~Jid();

  const std::string & node() const { return node_name_; }
  const std::string & domain() const { return domain_name_;  }
  const std::string & resource() const { return resource_name_; }

  std::string Str() const;
  Jid BareJid() const;

  bool IsEmpty() const;
  bool IsValid() const;
  bool IsBare() const;
  bool IsFull() const;

  bool BareEquals(const Jid& other) const;
  void CopyFrom(const Jid& jid);
  bool operator==(const Jid& other) const;
  bool operator!=(const Jid& other) const { return !operator==(other); }

  bool operator<(const Jid& other) const { return Compare(other) < 0; };
  bool operator>(const Jid& other) const { return Compare(other) > 0; };

  int Compare(const Jid & other) const;

private:
  void ValidateOrReset();

  static std::string PrepNode(const std::string& node, bool* valid);
  static char PrepNodeAscii(char ch, bool* valid);
  static std::string PrepResource(const std::string& start, bool* valid);
  static char PrepResourceAscii(char ch, bool* valid);
  static std::string PrepDomain(const std::string& domain, bool* valid);
  static void PrepDomain(const std::string& domain,
                         std::string* buf, bool* valid);
  static void PrepDomainLabel(
      std::string::const_iterator start, std::string::const_iterator end,
      std::string* buf, bool* valid);
  static char PrepDomainLabelAscii(char ch, bool *valid);

  std::string node_name_;
  std::string domain_name_;
  std::string resource_name_;
};

}

#endif  // TALK_XMPP_JID_H_
