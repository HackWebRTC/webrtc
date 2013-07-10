/*
 * libjingle
 * Copyright 2006, Google Inc.
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

#ifndef TALK_BASE_SIGSLOTREPEATER_H__
#define TALK_BASE_SIGSLOTREPEATER_H__

// repeaters are both signals and slots, which are designed as intermediate
// pass-throughs for signals and slots which don't know about each other (for
// modularity or encapsulation).  This eliminates the need to declare a signal
// handler whose sole purpose is to fire another signal.  The repeater connects
// to the originating signal using the 'repeat' method.  When the repeated
// signal fires, the repeater will also fire.

#include "talk/base/sigslot.h"

namespace sigslot {

  template<class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
  class repeater0 : public signal0<mt_policy>,
                    public has_slots<mt_policy>
  {
  public:
    typedef signal0<mt_policy> base_type;
    typedef repeater0<mt_policy> this_type;

    repeater0() { }
    repeater0(const this_type& s) : base_type(s) { }

    void reemit() { signal0<mt_policy>::emit(); }
    void repeat(base_type &s) { s.connect(this, &this_type::reemit); }
    void stop(base_type &s) { s.disconnect(this); }
  };

  template<class arg1_type, class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
  class repeater1 : public signal1<arg1_type, mt_policy>,
                    public has_slots<mt_policy>
  {
  public:
    typedef signal1<arg1_type, mt_policy> base_type;
    typedef repeater1<arg1_type, mt_policy> this_type;

    repeater1() { }
    repeater1(const this_type& s) : base_type(s) { }

    void reemit(arg1_type a1) { signal1<arg1_type, mt_policy>::emit(a1); }
    void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
    void stop(base_type &s) { s.disconnect(this); }
  };

  template<class arg1_type, class arg2_type, class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
  class repeater2 : public signal2<arg1_type, arg2_type, mt_policy>,
                    public has_slots<mt_policy>
  {
  public:
    typedef signal2<arg1_type, arg2_type, mt_policy> base_type;
    typedef repeater2<arg1_type, arg2_type, mt_policy> this_type;

    repeater2() { }
    repeater2(const this_type& s) : base_type(s) { }

    void reemit(arg1_type a1, arg2_type a2) { signal2<arg1_type, arg2_type, mt_policy>::emit(a1,a2); }
    void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
    void stop(base_type &s) { s.disconnect(this); }
  };

  template<class arg1_type, class arg2_type, class arg3_type,
           class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
  class repeater3 : public signal3<arg1_type, arg2_type, arg3_type, mt_policy>,
                    public has_slots<mt_policy>
  {
  public:
    typedef signal3<arg1_type, arg2_type, arg3_type, mt_policy> base_type;
    typedef repeater3<arg1_type, arg2_type, arg3_type, mt_policy> this_type;

    repeater3() { }
    repeater3(const this_type& s) : base_type(s) { }

    void reemit(arg1_type a1, arg2_type a2, arg3_type a3) {
            signal3<arg1_type, arg2_type, arg3_type, mt_policy>::emit(a1,a2,a3);
    }
    void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
    void stop(base_type &s) { s.disconnect(this); }
  };

}  // namespace sigslot

#endif  // TALK_BASE_SIGSLOTREPEATER_H__
