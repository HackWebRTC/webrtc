// Copyright 2011 Google Inc. All Rights Reserved.


#include "talk/xmpp/pingtask.h"

#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/constants.h"

namespace buzz {

PingTask::PingTask(buzz::XmppTaskParentInterface* parent,
                   talk_base::MessageQueue* message_queue,
                   uint32 ping_period_millis,
                   uint32 ping_timeout_millis)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      message_queue_(message_queue),
      ping_period_millis_(ping_period_millis),
      ping_timeout_millis_(ping_timeout_millis),
      next_ping_time_(0),
      ping_response_deadline_(0) {
  ASSERT(ping_period_millis >= ping_timeout_millis);
}

bool PingTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, Jid(STR_EMPTY), task_id())) {
    return false;
  }

  if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_RESULT &&
      stanza->Attr(buzz::QN_TYPE) != buzz::STR_ERROR) {
    return false;
  }

  QueueStanza(stanza);
  return true;
}

// This task runs indefinitely and remains in either the start or blocked
// states.
int PingTask::ProcessStart() {
  if (ping_period_millis_ < ping_timeout_millis_) {
    LOG(LS_ERROR) << "ping_period_millis should be >= ping_timeout_millis";
    return STATE_ERROR;
  }
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza != NULL) {
    // Received a ping response of some sort (don't care what it is).
    ping_response_deadline_ = 0;
  }

  uint32 now = talk_base::Time();

  // If the ping timed out, signal.
  if (ping_response_deadline_ != 0 && now >= ping_response_deadline_) {
    SignalTimeout();
    return STATE_ERROR;
  }

  // Send a ping if it's time.
  if (now >= next_ping_time_) {
    talk_base::scoped_ptr<buzz::XmlElement> stanza(
        MakeIq(buzz::STR_GET, Jid(STR_EMPTY), task_id()));
    stanza->AddElement(new buzz::XmlElement(QN_PING));
    SendStanza(stanza.get());

    ping_response_deadline_ = now + ping_timeout_millis_;
    next_ping_time_ = now + ping_period_millis_;

    // Wake ourselves up when it's time to send another ping or when the ping
    // times out (so we can fire a signal).
    message_queue_->PostDelayed(ping_timeout_millis_, this);
    message_queue_->PostDelayed(ping_period_millis_, this);
  }

  return STATE_BLOCKED;
}

void PingTask::OnMessage(talk_base::Message* msg) {
  // Get the task manager to run this task so we can send a ping or signal or
  // process a ping response.
  Wake();
}

} // namespace buzz
