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

// LibjinglePlus is a class that connects to Google Talk, creates
// some common tasks, and emits signals when things change

#ifndef LIBJINGLEPLUS_H__
#define LIBJINGLEPLUS_H__

#include "talk/base/basicdefs.h"
#include "talk/app/rosteritem.h"
#include "talk/app/message.h"
#include "talk/app/status.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/base/scoped_ptr.h"


class LibjinglePlusWorker;

class LibjinglePlusNotify {
 public:
  virtual ~LibjinglePlusNotify() {}

  /* Libjingle+ works on its own thread. It will call WakeupMainThread
   * when it has something to report. The main thread should then wake up,
   * and call DoCallbacks on the LibjinglePlus object.
   *
   * This function gets called from libjingle+'s worker thread. All other
   * methods in LibjinglePlusNotify get called from the thread you call
   * DoCallbacks() on.
   *
   * If running on Windows, libjingle+ will use Windows messages to generate
   * callbacks from the main thread, and you don't need to do anything here.
   */
  virtual void WakeupMainThread() = 0;

  /* Connection */
  /* Called when the connection state changes */
  virtual void OnStateChange(buzz::XmppEngine::State) = 0;

  /* Called when the socket closes */
  virtual void OnSocketClose(int error_code) = 0;

  /* Called when XMPP is being sent or received. Used for debugging */
  virtual void OnXmppOutput(const std::string &output) = 0;
  virtual void OnXmppInput(const std::string &input) = 0;

  /* Presence */
  /* Called when someone's Status is updated */
  virtual void OnStatusUpdate(const buzz::Status &status) = 0;

  /* Called when a status update results in an error */
  virtual void OnStatusError(const buzz::XmlElement &stanza) = 0;

  /* Called with an IQ return code */
  virtual void OnIqDone(bool success, const buzz::XmlElement &stanza) = 0;

  /* Message */
  /* Called when a message comes in. */
  virtual void OnMessage(const buzz::XmppMessage &message) = 0;

  /* Roster */

  /* Called when we start refreshing the roster */
  virtual void OnRosterRefreshStarted() = 0;
  /* Called when we have the entire roster */
  virtual void OnRosterRefreshFinished() = 0;
  /* Called when an item on the roster is created or updated */
  virtual void OnRosterItemUpdated(const buzz::RosterItem &ri) = 0;
  /* Called when an item on the roster is removed */
  virtual void OnRosterItemRemoved(const buzz::RosterItem &ri) = 0;

  /* Subscriptions */
  virtual void OnRosterSubscribe(const buzz::Jid &jid) = 0;
  virtual void OnRosterUnsubscribe(const buzz::Jid &jid) = 0;
  virtual void OnRosterSubscribed(const buzz::Jid &jid) = 0;
  virtual void OnRosterUnsubscribed(const buzz::Jid &jid) = 0;

};

class LibjinglePlus 
{
 public:
  /* Provide the constructor with your interface. */
  LibjinglePlus(LibjinglePlusNotify *notify);
  ~LibjinglePlus();
 
  /* Logs in and starts doing stuff 
   *
   * If cookie_auth is true, password must be a Gaia SID. Otherwise,
   * it should be the user's password
   */
  void Login(const std::string &username, const std::string &password,
	     const std::string &machine_address, bool is_test, bool cookie_auth);

  /* Set Presence */
  void SendPresence(const buzz::Status & s);
  void SendDirectedPresence(const buzz::Jid & j, const buzz::Status & s);
  void SendDirectedMUCPresence(const buzz::Jid & j, const buzz::Status & s, 
		       const std::string &user_nick, const std::string &api_capability,
		       const std::string &api_message, const std::string &role);

  /* Send Message */
  void SendXmppMessage(const buzz::XmppMessage & m);

  /* Send IQ */
  void SendXmppIq(const buzz::Jid &to_jid, bool is_get,
                  const buzz::XmlElement *iq_element);

  /* Set Roster */
  void UpdateRosterItem(const buzz::Jid & jid, const std::string & name, 
			const std::vector<std::string> & groups, buzz::GrType grt);
  void RemoveRosterItem(const buzz::Jid &jid);

  /* Call this from the thread you want to receive callbacks on. Typically, this will be called
   * after your WakeupMainThread() notify function is called.
   *
   * On Windows, libjingle+ will trigger its callback from the Windows message loop, and
   * you needn't call this yourself.
   */
  void DoCallbacks();

 private:
  void LoginInternal(const std::string &jid, const std::string &password,
		     const std::string &machine_address, bool is_test);

  LibjinglePlusWorker *worker_;
};

#endif  // LIBJINGLE_PLUS_H__
