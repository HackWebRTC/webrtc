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
#include "talk/examples/chat/chatapp.h"

#include "talk/examples/chat/consoletask.h"
#include "talk/examples/chat/textchatsendtask.h"
#include "talk/examples/chat/textchatreceivetask.h"
#include "talk/xmpp/presenceouttask.h"
#include "talk/xmpp/presencereceivetask.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

ChatApp::ChatApp(buzz::XmppClient* xmpp_client, talk_base::Thread* main_thread)
  : xmpp_client_(xmpp_client),
    presence_out_task_(NULL),
    presence_receive_task_(NULL),
    message_send_task_(NULL),
    message_received_task_(NULL),
    console_task_(new buzz::ConsoleTask(main_thread)),
    ui_state_(STATE_BASE) {
  xmpp_client_->SignalStateChange.connect(this, &ChatApp::OnStateChange);

  console_task_->TextInputHandler.connect(this, &ChatApp::OnConsoleMessage);
  console_task_->Start();
}

ChatApp::~ChatApp() {
  if (presence_out_task_ != NULL) {
    // Check out
    BroadcastPresence(away);
  }
}

void ChatApp::Quit() {
  talk_base::Thread::Current()->Quit();
}

void ChatApp::OnXmppOpen() {
  presence_out_task_.reset(new buzz::PresenceOutTask(xmpp_client_));
  presence_receive_task_.reset(new buzz::PresenceReceiveTask(xmpp_client_));
  presence_receive_task_->PresenceUpdate.connect(this,
                                                 &ChatApp::OnPresenceUpdate);
  message_send_task_.reset(new buzz::TextChatSendTask(xmpp_client_));
  message_received_task_.reset(new buzz::TextChatReceiveTask(xmpp_client_));
  message_received_task_->SignalTextChatReceived.connect(
      this, &ChatApp::OnTextMessage);

  presence_out_task_->Start();
  presence_receive_task_->Start();
  message_send_task_->Start();
  message_received_task_->Start();
}

void ChatApp::BroadcastPresence(PresenceState state) {
  buzz::PresenceStatus status;
  status.set_jid(xmpp_client_->jid());
  status.set_available(state == online);
  status.set_show(state == online ? buzz::PresenceStatus::SHOW_ONLINE
                                  : buzz::PresenceStatus::SHOW_AWAY);
  presence_out_task_->Send(status);
}

// UI Stuff
static const char* kMenuChoiceQuit = "0";
static const char* kMenuChoiceRoster = "1";
static const char* kMenuChoiceChat = "2";

static const char* kUIStrings[3][2] = {
  {kMenuChoiceQuit, "Quit"},
  {kMenuChoiceRoster, "Roster"},
  {kMenuChoiceChat, "Send"}};

void ChatApp::PrintMenu() {
  char buff[128];
  int numMenuItems = sizeof(kUIStrings) / sizeof(kUIStrings[0]);
  for (int index = 0; index < numMenuItems; ++index) {
    snprintf(buff, sizeof(buff), "%s) %s\n", kUIStrings[index][0],
                                             kUIStrings[index][1]);
    console_task_->Print(buff);
  }
  console_task_->Print("choice:");
}

void ChatApp::PrintRoster() {
  int index = 0;
  for (RosterList::iterator iter = roster_list_.begin();
     iter != roster_list_.end(); ++iter) {
       const buzz::Jid& jid = iter->second.jid();
       console_task_->Print(
         "%d: (*) %s@%s [%s] \n",
         index++,
         jid.node().c_str(),
         jid.domain().c_str(),
         jid.resource().c_str());
  }
}

void ChatApp::PromptJid() {
  PrintRoster();
  console_task_->Print("choice:");
}

void ChatApp::PromptChatMessage() {
  console_task_->Print(":");
}

bool ChatApp::GetRosterItem(int index, buzz::PresenceStatus* status) {
  int found_index = 0;
  for (RosterList::iterator iter = roster_list_.begin();
     iter != roster_list_.end() && found_index <= index; ++iter) {
    if (found_index == index) {
      *status = iter->second;
      return true;
    }
    found_index++;
  }

  return false;
}

void ChatApp::HandleBaseInput(const std::string& message) {
  if (message == kMenuChoiceQuit) {
    Quit();
  } else if (message == kMenuChoiceRoster) {
    PrintRoster();
  } else if (message == kMenuChoiceChat) {
    ui_state_ = STATE_PROMPTJID;
    PromptJid();
  } else if (message == "") {
    PrintMenu();
  }
}

void ChatApp::HandleJidInput(const std::string& message) {
  if (isdigit(message[0])) {
    // It's an index-based roster choice.
    int index = 0;
    buzz::PresenceStatus status;
    if (!talk_base::FromString(message, &index) ||
        !GetRosterItem(index, &status)) {
      // fail, so drop back
      ui_state_ = STATE_BASE;
      return;
    }

    chat_dest_jid_ = status.jid();
  } else {
    // It's an explicit address.
    chat_dest_jid_ = buzz::Jid(message.c_str());
  }
  ui_state_ = STATE_CHATTING;
  PromptChatMessage();
}

void ChatApp::HandleChatInput(const std::string& message) {
  if (message == "") {
    ui_state_ = STATE_BASE;
    PrintMenu();
  } else {
    message_send_task_->Send(chat_dest_jid_, message);
    PromptChatMessage();
  }
}

// Connection state notifications
void ChatApp::OnStateChange(buzz::XmppEngine::State state) {
  switch (state) {
  // Nonexistent state
  case buzz::XmppEngine::STATE_NONE:
    break;

  // Nonexistent state
  case buzz::XmppEngine::STATE_START:
    break;

  // Exchanging stream headers, authenticating and so on.
  case buzz::XmppEngine::STATE_OPENING:
    break;

  // Authenticated and bound.
  case buzz::XmppEngine::STATE_OPEN:
    OnXmppOpen();
    BroadcastPresence(online);
    PrintMenu();
    break;

  // Session closed, possibly due to error.
  case buzz::XmppEngine::STATE_CLOSED:
    break;
  }
}

// Presence Notifications
void ChatApp::OnPresenceUpdate(const buzz::PresenceStatus& status) {
  if (status.available()) {
    roster_list_[status.jid().Str()] = status;
  } else {
    RosterList::iterator iter = roster_list_.find(status.jid().Str());
    if (iter != roster_list_.end()) {
      roster_list_.erase(iter);
    }
  }
}

// Text message handlers
void ChatApp::OnTextMessage(const buzz::Jid& from, const buzz::Jid& to,
                            const std::string& message) {
  console_task_->Print("%s says: %s\n", from.node().c_str(), message.c_str());
}

void ChatApp::OnConsoleMessage(const std::string &message) {
  switch (ui_state_) {
    case STATE_BASE:
      HandleBaseInput(message);
      break;

    case STATE_PROMPTJID:
      HandleJidInput(message);
      break;

    case STATE_CHATTING:
      HandleChatInput(message);
      break;
  }
}
