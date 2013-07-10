// Copyright 2012 Google Inc. All Rights Reserved.


#ifndef TALK_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_
#define TALK_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_

#include "talk/xmpp/iqtask.h"

namespace buzz {

// Task to request a unique hangout id to be used when starting a hangout.
// The protocol is described in https://docs.google.com/a/google.com/
// document/d/1EFLT6rCYPDVdqQXSQliXwqB3iUkpZJ9B_MNFeOZgN7g/edit
class MucRoomUniqueHangoutIdTask : public buzz::IqTask {
 public:
  MucRoomUniqueHangoutIdTask(buzz::XmppTaskParentInterface* parent,
                        const Jid& lookup_server_jid);
  // signal(task, hangout_id)
  sigslot::signal2<MucRoomUniqueHangoutIdTask*, const std::string&> SignalResult;

 protected:
  virtual void HandleResult(const buzz::XmlElement* stanza);

 private:
  static buzz::XmlElement* MakeUniqueRequestXml();

};

} // namespace buzz

#endif  // TALK_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_
