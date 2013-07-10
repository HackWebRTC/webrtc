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

#include <iostream>
#include <string>

#include "talk/libjingle-plus/libjingleplus.h"

class Notifier : virtual public LibjinglePlusNotify {
  virtual void OnStateChange(buzz::XmppEngine::State state) {
    std::cout << "State change: " << state << std::endl;
  }

  virtual void OnSocketClose(int error_code) {
    std::cout << "Socket close: " << error_code << std::endl;
  }

  virtual void OnXmppOutput(const std::string &output) {
    std::cout << ">>>>>>>>" << std::endl << output << std::endl << ">>>>>>>>" << std::endl;
  }
  
  virtual void OnXmppInput(const std::string &input) {
    std::cout << "<<<<<<<<" << std::endl << input << std::endl << "<<<<<<<<" << std::endl;
  } 

  
  virtual void OnStatusUpdate(const buzz::Status &status) {
    std::string from = status.jid().Str();
    std::cout  << from  << " - " << status.status() << std::endl;
  }

  virtual void OnStatusError(const buzz::XmlElement &stanza) {
  }
  
  virtual void OnIqDone(bool success, const buzz::XmlElement &stanza) {
  }

  virtual void OnMessage(const buzz::XmppMessage &m) {
    if (m.body() != "")
      std::cout << m.from().Str() << ": " << m.body() << std::endl;
  }

  void OnRosterItemUpdated(const buzz::RosterItem &ri) {
    std::cout << "Roster item: " << ri.jid().Str() << std::endl;
  }
  
  virtual void OnRosterItemRemoved(const buzz::RosterItem &ri) {
    std::cout << "Roster item removed: " << ri.jid().Str() << std::endl;
  }

  virtual void OnRosterSubscribe(const buzz::Jid& jid) {
    std::cout << "Subscribing: " << jid.Str() << std::endl;
  }

  virtual void OnRosterUnsubscribe(const buzz::Jid &jid) {
    std::cout << "Unsubscribing: " <<jid.Str() << std::endl;
  }
  
  virtual void OnRosterSubscribed(const buzz::Jid &jid) {
    std::cout << "Subscribed: " << jid.Str() << std::endl;
  }
  
  virtual void OnRosterUnsubscribed(const buzz::Jid &jid) {
    std::cout << "Unsubscribed: " << jid.Str() << std::endl;
  }

  virtual void OnRosterRefreshStarted() {
    std::cout << "Refreshing roster." << std::endl;
  }
  
  virtual void OnRosterRefreshFinished() {
    std::cout << "Roster refreshed." << std::endl;
  }

  virtual void WakeupMainThread() {
  }
};
