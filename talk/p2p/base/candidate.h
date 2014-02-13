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

#ifndef TALK_P2P_BASE_CANDIDATE_H_
#define TALK_P2P_BASE_CANDIDATE_H_

#include <climits>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

#include "talk/base/basictypes.h"
#include "talk/base/socketaddress.h"
#include "talk/p2p/base/constants.h"

namespace cricket {

// Candidate for ICE based connection discovery.

class Candidate {
 public:
  // TODO: Match the ordering and param list as per RFC 5245
  // candidate-attribute syntax. http://tools.ietf.org/html/rfc5245#section-15.1
  Candidate() : component_(0), priority_(0), generation_(0) {}
  Candidate(const std::string& id, int component, const std::string& protocol,
            const talk_base::SocketAddress& address, uint32 priority,
            const std::string& username, const std::string& password,
            const std::string& type, const std::string& network_name,
            uint32 generation, const std::string& foundation)
      : id_(id), component_(component), protocol_(protocol), address_(address),
        priority_(priority), username_(username), password_(password),
        type_(type), network_name_(network_name), generation_(generation),
        foundation_(foundation) {
  }

  const std::string & id() const { return id_; }
  void set_id(const std::string & id) { id_ = id; }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  const std::string & protocol() const { return protocol_; }
  void set_protocol(const std::string & protocol) { protocol_ = protocol; }

  const talk_base::SocketAddress & address() const { return address_; }
  void set_address(const talk_base::SocketAddress & address) {
    address_ = address;
  }

  uint32 priority() const { return priority_; }
  void set_priority(const uint32 priority) { priority_ = priority; }

//  void set_type_preference(uint32 type_preference) {
//    priority_ = GetPriority(type_preference);
//  }

  // Maps old preference (which was 0.0-1.0) to match priority (which
  // is 0-2^32-1) to to match RFC 5245, section 4.1.2.1.  Also see
  // https://docs.google.com/a/google.com/document/d/
  // 1iNQDiwDKMh0NQOrCqbj3DKKRT0Dn5_5UJYhmZO-t7Uc/edit
  float preference() const {
    // The preference value is clamped to two decimal precision.
    return static_cast<float>(((priority_ >> 24) * 100 / 127) / 100.0);
  }

  void set_preference(float preference) {
    // Limiting priority to UINT_MAX when value exceeds uint32 max.
    // This can happen for e.g. when preference = 3.
    uint64 prio_val = static_cast<uint64>(preference * 127) << 24;
    priority_ = static_cast<uint32>(
      talk_base::_min(prio_val, static_cast<uint64>(UINT_MAX)));
  }

  const std::string & username() const { return username_; }
  void set_username(const std::string & username) { username_ = username; }

  const std::string & password() const { return password_; }
  void set_password(const std::string & password) { password_ = password; }

  const std::string & type() const { return type_; }
  void set_type(const std::string & type) { type_ = type; }

  const std::string & network_name() const { return network_name_; }
  void set_network_name(const std::string & network_name) {
    network_name_ = network_name;
  }

  // Candidates in a new generation replace those in the old generation.
  uint32 generation() const { return generation_; }
  void set_generation(uint32 generation) { generation_ = generation; }
  const std::string generation_str() const {
    std::ostringstream ost;
    ost << generation_;
    return ost.str();
  }
  void set_generation_str(const std::string& str) {
    std::istringstream ist(str);
    ist >> generation_;
  }

  const std::string& foundation() const {
    return foundation_;
  }

  void set_foundation(const std::string& foundation) {
    foundation_ = foundation;
  }

  const talk_base::SocketAddress & related_address() const {
    return related_address_;
  }
  void set_related_address(
      const talk_base::SocketAddress & related_address) {
    related_address_ = related_address;
  }

  // Determines whether this candidate is equivalent to the given one.
  bool IsEquivalent(const Candidate& c) const {
    // We ignore the network name, since that is just debug information, and
    // the priority, since that should be the same if the rest is (and it's
    // a float so equality checking is always worrisome).
    return (id_ == c.id_) &&
           (component_ == c.component_) &&
           (protocol_ == c.protocol_) &&
           (address_ == c.address_) &&
           (username_ == c.username_) &&
           (password_ == c.password_) &&
           (type_ == c.type_) &&
           (generation_ == c.generation_) &&
           (foundation_ == c.foundation_) &&
           (related_address_ == c.related_address_);
  }

  std::string ToString() const {
    return ToStringInternal(false);
  }

  std::string ToSensitiveString() const {
    return ToStringInternal(true);
  }

  uint32 GetPriority(uint32 type_preference,
                     int network_adapter_preference) const {
    // RFC 5245 - 4.1.2.1.
    // priority = (2^24)*(type preference) +
    //            (2^8)*(local preference) +
    //            (2^0)*(256 - component ID)

    // |local_preference| length is 2 bytes, 0-65535 inclusive.
    // In our implemenation we will partion local_preference into
    //              0                 1
    //       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |  NIC Pref     |    Addr Pref  |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // NIC Type - Type of the network adapter e.g. 3G/Wifi/Wired.
    // Addr Pref - Address preference value as per RFC 3484.
    // local preference is calculated as - NIC Type << 8 | Addr_Pref.

    int addr_pref = IPAddressPrecedence(address_.ipaddr());
    int local_preference = (network_adapter_preference << 8) | addr_pref;

    return (type_preference << 24) |
           (local_preference << 8) |
           (256 - component_);
  }

 private:
  std::string ToStringInternal(bool sensitive) const {
    std::ostringstream ost;
    std::string address = sensitive ? address_.ToSensitiveString() :
                                      address_.ToString();
    ost << "Cand[" << foundation_ << ":" << component_ << ":"
        << protocol_ << ":" << priority_ << ":"
        << address << ":" << type_ << ":" << related_address_ << ":"
        << username_ << ":" << password_ << "]";
    return ost.str();
  }

  std::string id_;
  int component_;
  std::string protocol_;
  talk_base::SocketAddress address_;
  uint32 priority_;
  std::string username_;
  std::string password_;
  std::string type_;
  std::string network_name_;
  uint32 generation_;
  std::string foundation_;
  talk_base::SocketAddress related_address_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_CANDIDATE_H_
