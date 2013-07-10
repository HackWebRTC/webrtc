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

#include <cassert>

#include "talk/base/nattypes.h"

namespace talk_base {

class SymmetricNAT : public NAT {
public:
  bool IsSymmetric() { return true; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return true; }
};

class OpenConeNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return false; }
  bool FiltersPort() { return false; }
};

class AddressRestrictedNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return false; }
};

class PortRestrictedNAT : public NAT {
public:
  bool IsSymmetric() { return false; }
  bool FiltersIP() { return true; }
  bool FiltersPort() { return true; }
};

NAT* NAT::Create(NATType type) {
  switch (type) {
  case NAT_OPEN_CONE:       return new OpenConeNAT();
  case NAT_ADDR_RESTRICTED: return new AddressRestrictedNAT();
  case NAT_PORT_RESTRICTED: return new PortRestrictedNAT();
  case NAT_SYMMETRIC:       return new SymmetricNAT();
  default: assert(0);       return 0;
  }
}

} // namespace talk_base
