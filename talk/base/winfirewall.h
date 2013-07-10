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

#ifndef TALK_BASE_WINFIREWALL_H_
#define TALK_BASE_WINFIREWALL_H_

#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;  // Can't forward declare typedef, but don't need all win
#endif // !_HRESULT_DEFINED

struct INetFwMgr;
struct INetFwPolicy;
struct INetFwProfile;

namespace talk_base {

//////////////////////////////////////////////////////////////////////
// WinFirewall
//////////////////////////////////////////////////////////////////////

class WinFirewall {
 public:
  WinFirewall();
  ~WinFirewall();

  bool Initialize(HRESULT* result);
  void Shutdown();

  bool Enabled() const;
  bool QueryAuthorized(const char* filename, bool* authorized) const;
  bool QueryAuthorizedW(const wchar_t* filename, bool* authorized) const;

  bool AddApplication(const char* filename, const char* friendly_name,
                      bool authorized, HRESULT* result);
  bool AddApplicationW(const wchar_t* filename, const wchar_t* friendly_name,
                       bool authorized, HRESULT* result);

 private:
  INetFwMgr* mgr_;
  INetFwPolicy* policy_;
  INetFwProfile* profile_;
};

//////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // TALK_BASE_WINFIREWALL_H_
