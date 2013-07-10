/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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

#ifndef TALK_EXAMPLES_CHAT_CHATAPP_H_
#define TALK_EXAMPLES_CHAT_CHATAPP_H_

#include "talk/base/thread.h"
#include "talk/base/scoped_ptr.h"

#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"

namespace buzz {
class XmppClient;
class PresenceOutTask;
class PresenceReceiveTask;
class TextChatSendTask;
class TextChatReceiveTask;
class ConsoleTask;
class PresenceStatus;
}

// This is an example chat app for libjingle, showing how to use xmpp tasks,
// data, callbacks, etc.  It has a simple text-based UI for logging in,
// sending and receiving messages, and printing the roster.
class ChatApp: public sigslot::has_slots<> {
 public:
  // Arguments:
  //   xmpp_client  Points to the XmppClient for the communication channel
  //    (typically created by the XmppPump object).
  //   main_thread  Wraps the application's main thread.  Subsidiary threads
  //    for the various tasks will be forked off of this.
  ChatApp(buzz::XmppClient* xmpp_client, talk_base::Thread* main_thread);

  // Shuts down and releases all of the contained tasks/threads
  ~ChatApp();

  // Shuts down the current thread and quits
  void Quit();

 private:
  //
  // Initialization
  //
  // Called explicitly after the connection to the chat server is established.
  void OnXmppOpen();

  //
  // UI Stuff
  //
  // Prints the app main menu on the console.
  // Called when ui_state_ == STATE_BASE.
  void PrintMenu();

  // Prints a numbered list of the logged-in user's roster on the console.
  void PrintRoster();

  // Prints a prompt for the user to enter either the index from the
  // roster list of the user they wish to chat with, or a fully-qualified
  // (user@server.ext) jid.
  // Called when when ui_state_ == STATE_PROMPTJID.
  void PromptJid();

  // Prints a prompt on the console for the user to enter a message to send.
  // Called when when ui_state_ == STATE_CHATTING.
  void PromptChatMessage();

  // Sends our presence state to the chat server (and on to your roster list).
  // Arguments:
  //  state Specifies the presence state to show.
  enum PresenceState {online, away};
  void BroadcastPresence(PresenceState state);

  // Returns the RosterItem associated with the specified index.
  // Just a helper to select a roster item from a numbered list in the UI.
  bool GetRosterItem(int index, buzz::PresenceStatus* status);

  //
  // Input Handling
  //
  // Receives input when ui_state_ == STATE_BASE.  Handles choices from the
  // main menu.
  void HandleBaseInput(const std::string& message);

  // Receives input when ui_state_ == STATE_PROMPTJID.  Handles selection
  // of a JID to chat to.
  void HandleJidInput(const std::string& message);

  // Receives input when ui_state_ == STATE_CHATTING.  Handles text messages.
  void HandleChatInput(const std::string& message);

  //
  // signal/slot Callbacks
  //
  // Connected to the XmppClient::SignalStateChange slot.  Receives
  // notifications of state changes of the connection.
  void OnStateChange(buzz::XmppEngine::State state);

  // Connected to the PresenceReceiveTask::PresenceUpdate slot.
  // Receives status messages for the logged-in user's roster (i.e.
  // an initial list from the server and people coming/going).
  void OnPresenceUpdate(const buzz::PresenceStatus& status);

  // Connected to the TextChatReceiveTask::SignalTextChatReceived slot.
  // Called when we receive a text chat from someone else.
  void OnTextMessage(const buzz::Jid& from, const buzz::Jid& to,
                     const std::string& message);

  // Receives text input from the console task.  This is where any input
  // from the user comes in.
  // Arguments:
  //   message What the user typed.
  void OnConsoleMessage(const std::string &message);

  // The XmppClient object associated with this chat application instance.
  buzz::XmppClient* xmpp_client_;

  // We send presence information through this object.
  talk_base::scoped_ptr<buzz::PresenceOutTask> presence_out_task_;

  // We receive others presence information through this object.
  talk_base::scoped_ptr<buzz::PresenceReceiveTask> presence_receive_task_;

  // We send text messages though this object.
  talk_base::scoped_ptr<buzz::TextChatSendTask> message_send_task_;

  // We receive messages through this object.
  talk_base::scoped_ptr<buzz::TextChatReceiveTask> message_received_task_;

  // UI gets drawn and receives input through this task.
  talk_base::scoped_ptr< buzz::ConsoleTask> console_task_;

  // The list of JIDs for the people in the logged-in users roster.
  // RosterList  roster_list_;
  typedef std::map<std::string, buzz::PresenceStatus> RosterList;
  RosterList roster_list_;

  // The JID of the user currently being chatted with.
  buzz::Jid chat_dest_jid_;

  // UI State constants
  enum UIState { STATE_BASE, STATE_PROMPTJID, STATE_CHATTING };
  UIState ui_state_;
};

#endif  // TALK_EXAMPLES_CHAT_CHATAPP_H_

