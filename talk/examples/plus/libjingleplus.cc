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
#include "libjingleplus.h"
#ifdef WIN32
#include "talk/base/win32socketserver.h"
#endif
#include "talk/base/physicalsocketserver.h"
#include "talk/base/logging.h"
#include "talk/examples/login/xmppauth.h"
#include "talk/examples/login/xmppsocket.h"
#include "talk/examples/login/xmpppump.h"
#include "presencepushtask.h"
#include "talk/app/status.h"
#include "talk/app/message.h"
#include "rostertask.h"
#include "talk/app/iqtask.h"
#include "talk/app/presenceouttask.h"
#include "talk/app/receivemessagetask.h"
#include "talk/app/rostersettask.h"
#include "talk/app/sendmessagetask.h"

enum {
  MSG_START,
  
  // main thread to worker
  MSG_LOGIN,
  MSG_DISCONNECT,
  MSG_SEND_PRESENCE,
  MSG_SEND_DIRECTED_PRESENCE,
  MSG_SEND_DIRECTED_MUC_PRESENCE,
  MSG_SEND_XMPP_MESSAGE,
  MSG_SEND_XMPP_IQ,
  MSG_UPDATE_ROSTER_ITEM,
  MSG_REMOVE_ROSTER_ITEM,

  // worker thread to main thread
  MSG_STATE_CHANGE,
  MSG_STATUS_UPDATE,
  MSG_STATUS_ERROR,
  MSG_ROSTER_REFRESH_STARTED,
  MSG_ROSTER_REFRESH_FINISHED,
  MSG_ROSTER_ITEM_UPDATED,
  MSG_ROSTER_ITEM_REMOVED,
  MSG_ROSTER_SUBSCRIBE,
  MSG_ROSTER_UNSUBSCRIBE,
  MSG_ROSTER_SUBSCRIBED,
  MSG_ROSTER_UNSUBSCRIBED,
  MSG_INCOMING_MESSAGE,
  MSG_IQ_COMPLETE,
  MSG_XMPP_INPUT,
  MSG_XMPP_OUTPUT
};

class LibjinglePlusWorker : public talk_base::MessageHandler, 
			    public XmppPumpNotify,
                            public sigslot::has_slots<> {
 public:	
  LibjinglePlusWorker(LibjinglePlus *ljp, LibjinglePlusNotify *notify) : 
    worker_thread_(NULL), ljp_(ljp), notify_(notify),
    ppt_(NULL), rmt_(NULL), rt_(NULL), is_test_login_(false) {

    main_thread_.reset(new talk_base::AutoThread());
#ifdef WIN32
    ss_.reset(new talk_base::Win32SocketServer(main_thread_.get()));
    main_thread_->set_socketserver(ss_.get());
#endif

    pump_.reset(new XmppPump(this));

    pump_->client()->SignalLogInput.connect(this, &LibjinglePlusWorker::OnInputDebug);
    pump_->client()->SignalLogOutput.connect(this, &LibjinglePlusWorker::OnOutputDebug);
    //pump_->client()->SignalStateChange.connect(this, &LibjinglePlusWorker::OnStateChange);
    }

  ~LibjinglePlusWorker() {
    if (worker_thread_) {
      worker_thread_->Send(this, MSG_DISCONNECT);
      delete worker_thread_;
    }
  }
  
  virtual void OnMessage(talk_base::Message *msg) {
    switch (msg->message_id) {
    case MSG_START:
      LoginW();
      break;
    case MSG_DISCONNECT:
      DisconnectW();
      break;
    case MSG_SEND_XMPP_MESSAGE:
      SendXmppMessageW(static_cast<SendMessageData*>(msg->pdata)->m_);
      delete msg->pdata;
      break;
    case MSG_SEND_XMPP_IQ:
      SendXmppIqW(static_cast<SendIqData*>(msg->pdata)->to_jid_,
                  static_cast<SendIqData*>(msg->pdata)->is_get_,
                  static_cast<SendIqData*>(msg->pdata)->xml_element_);
      delete msg->pdata;
      break;
    case MSG_SEND_PRESENCE:
      SendPresenceW(static_cast<SendPresenceData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    case MSG_SEND_DIRECTED_PRESENCE:
      SendDirectedPresenceW(static_cast<SendDirectedPresenceData*>(msg->pdata)->j_,
			    static_cast<SendDirectedPresenceData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    case MSG_SEND_DIRECTED_MUC_PRESENCE:
      SendDirectedMUCPresenceW(static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->j_,
			       static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->s_,
			       static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->un_,
			       static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->ac_,
			       static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->am_,
			       static_cast<SendDirectedMUCPresenceData*>(msg->pdata)->role_);
      delete msg->pdata;
      break;
    case MSG_UPDATE_ROSTER_ITEM:
      UpdateRosterItemW(static_cast<UpdateRosterItemData*>(msg->pdata)->jid_,
			static_cast<UpdateRosterItemData*>(msg->pdata)->n_,
			static_cast<UpdateRosterItemData*>(msg->pdata)->g_,
			static_cast<UpdateRosterItemData*>(msg->pdata)->grt_);
      delete msg->pdata;
      break;
    case MSG_REMOVE_ROSTER_ITEM:
      RemoveRosterItemW(static_cast<JidData*>(msg->pdata)->jid_);
      delete msg->pdata;
      break;




    case MSG_STATUS_UPDATE:
      OnStatusUpdateW(static_cast<SendPresenceData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    case MSG_STATUS_ERROR:
      OnStatusErrorW(static_cast<StatusErrorData*>(msg->pdata)->stanza_);
      delete msg->pdata;
      break;
    case MSG_STATE_CHANGE:
      OnStateChangeW(static_cast<StateChangeData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_REFRESH_STARTED:
      OnRosterRefreshStartedW();
      break;
    case MSG_ROSTER_REFRESH_FINISHED:
      OnRosterRefreshFinishedW();
      break;
    case MSG_ROSTER_ITEM_UPDATED:
      OnRosterItemUpdatedW(static_cast<RosterItemData*>(msg->pdata)->ri_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_ITEM_REMOVED:
      OnRosterItemRemovedW(static_cast<RosterItemData*>(msg->pdata)->ri_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_SUBSCRIBE:
      OnRosterSubscribeW(static_cast<JidData*>(msg->pdata)->jid_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_UNSUBSCRIBE:
      OnRosterUnsubscribeW(static_cast<JidData*>(msg->pdata)->jid_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_SUBSCRIBED:
      OnRosterSubscribedW(static_cast<JidData*>(msg->pdata)->jid_);
      delete msg->pdata;
      break;
    case MSG_ROSTER_UNSUBSCRIBED:
      OnRosterUnsubscribedW(static_cast<JidData*>(msg->pdata)->jid_);
      delete msg->pdata;
      break;
    case MSG_INCOMING_MESSAGE:
      OnIncomingMessageW(static_cast<XmppMessageData*>(msg->pdata)->m_);
      delete msg->pdata;
      break;
    case MSG_IQ_COMPLETE:
      OnIqCompleteW(static_cast<IqCompleteData*>(msg->pdata)->success_,
                    static_cast<IqCompleteData*>(msg->pdata)->stanza_);
      delete msg->pdata;
      break;
    case MSG_XMPP_OUTPUT:
      OnOutputDebugW(static_cast<StringData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    case MSG_XMPP_INPUT:
      OnInputDebugW(static_cast<StringData*>(msg->pdata)->s_);
      delete msg->pdata;
      break;
    }
  }

  void Login(const std::string &jid, const std::string &password,
	     const std::string &machine_address, bool is_test, bool cookie_auth) {
    is_test_login_ = is_test;
  
    xcs_.set_user(jid);
    if (cookie_auth) {
	    xcs_.set_auth_cookie(password);
    } else {
	    talk_base::InsecureCryptStringImpl pass;
	    pass.password() = password;
	    xcs_.set_pass(talk_base::CryptString(pass));
    }
    xcs_.set_host(is_test ? "google.com" : "gmail.com");
    xcs_.set_resource("libjingleplus");
    xcs_.set_server(talk_base::SocketAddress(machine_address, 5222));
    xcs_.set_use_tls(!is_test);
    if (is_test) {
      xcs_.set_allow_plain(true);
    }

    worker_thread_ = new talk_base::Thread(&pss_);
    worker_thread_->Start(); 
    worker_thread_->Send(this, MSG_START);
  }
     
  void SendXmppMessage(const buzz::XmppMessage &m) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_SEND_XMPP_MESSAGE, new SendMessageData(m));
  }

  void SendXmppIq(const buzz::Jid &to_jid, bool is_get,
                  const buzz::XmlElement *xml_element) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_SEND_XMPP_IQ,
                         new SendIqData(to_jid, is_get, xml_element));
  }

  void SendPresence(const buzz::Status & s) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_SEND_PRESENCE, new SendPresenceData(s));
  }
  
  void SendDirectedPresence (const buzz::Jid &j, const buzz::Status &s) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_SEND_DIRECTED_PRESENCE, new SendDirectedPresenceData(j,s));
  }
  
  void SendDirectedMUCPresence(const buzz::Jid &j, const buzz::Status &s,
			       const std::string &un, const std::string &ac,
			       const std::string &am, const std::string &role) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_SEND_DIRECTED_MUC_PRESENCE, new SendDirectedMUCPresenceData(j,s,un,ac,am, role));
  }
  
  void UpdateRosterItem(const buzz::Jid & jid, const std::string & name, 
			const std::vector<std::string> & groups, buzz::GrType grt) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_UPDATE_ROSTER_ITEM, new UpdateRosterItemData(jid,name,groups,grt));
  }
  
  void RemoveRosterItemW(const buzz::Jid &jid) {
    buzz::RosterSetTask *rst = new buzz::RosterSetTask(pump_.get()->client());
    rst->Remove(jid);
    rst->Start();
  }

  void RemoveRosterItem(const buzz::Jid &jid) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    worker_thread_->Post(this, MSG_REMOVE_ROSTER_ITEM, new JidData(jid));
  }
  
  void DoCallbacks() {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    talk_base::Message m;
    while (main_thread_->Get(&m, 0)) {
      main_thread_->Dispatch(&m);
    }
  }

 private:

  struct UpdateRosterItemData : public talk_base::MessageData {
    UpdateRosterItemData(const buzz::Jid &jid, const std::string &name, 
			 const std::vector<std::string> &groups, buzz::GrType grt) :
      jid_(jid), n_(name), g_(groups), grt_(grt) {}
    buzz::Jid jid_;
    std::string n_;
    std::vector<std::string> g_;
    buzz::GrType grt_;
  };
  
  void UpdateRosterItemW(const buzz::Jid &jid, const std::string &name,
			 const std::vector<std::string> &groups, buzz::GrType grt) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::RosterSetTask *rst = new buzz::RosterSetTask(pump_.get()->client());
    rst->Update(jid, name, groups, grt);
    rst->Start();
  }

  struct StringData : public talk_base::MessageData {
    StringData(std::string s) : s_(s) {}
    std::string s_;
  };

  void OnInputDebugW(const std::string &data) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnXmppInput(data);
  }

  void OnInputDebug(const char *data, int len) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_XMPP_INPUT, new StringData(std::string(data,len)));
    if (notify_)
      notify_->WakeupMainThread();
  }  

  void OnOutputDebugW(const std::string &data) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnXmppOutput(data);
  }
  
  void OnOutputDebug(const char *data, int len) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_XMPP_OUTPUT, new StringData(std::string(data,len)));
    if (notify_)
      notify_->WakeupMainThread();
  }

  struct StateChangeData : public talk_base::MessageData {
    StateChangeData(buzz::XmppEngine::State state) : s_(state) {}
    buzz::XmppEngine::State s_;
  };
  
  void OnStateChange(buzz::XmppEngine::State state) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    switch (state) {
    case buzz::XmppEngine::STATE_OPEN:           
      ppt_ = new buzz::PresencePushTask(pump_.get()->client());
      ppt_->SignalStatusUpdate.connect(this,
                                       &LibjinglePlusWorker::OnStatusUpdate);
      ppt_->SignalStatusError.connect(this,
                                      &LibjinglePlusWorker::OnStatusError);
      ppt_->Start();

      rmt_ = new buzz::ReceiveMessageTask(pump_.get()->client(), buzz::XmppEngine::HL_ALL);
      rmt_->SignalIncomingMessage.connect(this, &LibjinglePlusWorker::OnIncomingMessage);
      rmt_->Start();

      rt_ = new buzz::RosterTask(pump_.get()->client());
      rt_->SignalRosterItemUpdated.connect(this, &LibjinglePlusWorker::OnRosterItemUpdated);
      rt_->SignalRosterItemRemoved.connect(this, &LibjinglePlusWorker::OnRosterItemRemoved);
      rt_->SignalSubscribe.connect(this, &LibjinglePlusWorker::OnRosterSubscribe);
      rt_->SignalUnsubscribe.connect(this, &LibjinglePlusWorker::OnRosterUnsubscribe);
      rt_->SignalSubscribed.connect(this, &LibjinglePlusWorker::OnRosterSubscribed);
      rt_->SignalUnsubscribed.connect(this, &LibjinglePlusWorker::OnRosterUnsubscribed);
      rt_->SignalRosterRefreshStarted.connect(this, &LibjinglePlusWorker::OnRosterRefreshStarted);
      rt_->SignalRosterRefreshFinished.connect(this, &LibjinglePlusWorker::OnRosterRefreshFinished);
      rt_->Start();
      rt_->RefreshRosterNow();

      break;
    }
    main_thread_->Post(this, MSG_STATE_CHANGE, new StateChangeData(state));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnStateChangeW(buzz::XmppEngine::State state) { 
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnStateChange(state);
  }

  struct RosterItemData : public talk_base::MessageData {
    RosterItemData(const buzz::RosterItem &ri) : ri_(ri) {}
    buzz::RosterItem ri_;
  };

  void OnRosterItemUpdatedW(const buzz::RosterItem &ri) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterItemUpdated(ri);
  }

  void OnRosterItemUpdated(const buzz::RosterItem &ri, bool huh) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_ITEM_UPDATED, new RosterItemData(ri));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnRosterItemRemovedW(const buzz::RosterItem &ri) { 
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterItemRemoved(ri);
  }
  
  void OnRosterItemRemoved(const buzz::RosterItem &ri) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_ITEM_REMOVED, new RosterItemData(ri));
    if (notify_)
      notify_->WakeupMainThread();
  }

  struct JidData : public talk_base::MessageData {
    JidData(const buzz::Jid& jid) : jid_(jid) {}
    const buzz::Jid jid_;
  };
  
  void OnRosterSubscribeW(const buzz::Jid& jid) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterSubscribe(jid);
  }

  void OnRosterSubscribe(const buzz::Jid& jid) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_SUBSCRIBE, new JidData(jid));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnRosterUnsubscribeW(const buzz::Jid &jid) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterUnsubscribe(jid);
  }

  void OnRosterUnsubscribe(const buzz::Jid &jid) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_UNSUBSCRIBE, new JidData(jid));
    if (notify_)
      notify_->WakeupMainThread();
  }
  
  void OnRosterSubscribedW(const buzz::Jid &jid) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterSubscribed(jid);
  }

  void OnRosterSubscribed(const buzz::Jid &jid) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_SUBSCRIBED, new JidData(jid));
    if (notify_)
      notify_->WakeupMainThread();
  }
  
  void OnRosterUnsubscribedW(const buzz::Jid &jid) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterUnsubscribed(jid);
  }

  void OnRosterUnsubscribed(const buzz::Jid &jid) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_UNSUBSCRIBED, new JidData(jid));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnRosterRefreshStartedW() {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterRefreshStarted();
  }
  
  void OnRosterRefreshStarted() {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_REFRESH_STARTED);
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnRosterRefreshFinishedW() {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnRosterRefreshFinished();
  }

  void OnRosterRefreshFinished() {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_ROSTER_REFRESH_FINISHED);
    if (notify_)
      notify_->WakeupMainThread();
  }
  
  struct XmppMessageData : talk_base::MessageData {
    XmppMessageData(const buzz::XmppMessage &m) : m_(m) {}
    buzz::XmppMessage m_;
  };
  
  void OnIncomingMessageW(const buzz::XmppMessage &msg) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnMessage(msg);
  }

  void OnIncomingMessage(const buzz::XmppMessage &msg) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_INCOMING_MESSAGE, new XmppMessageData(msg));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void OnStatusUpdateW (const buzz::Status &status) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnStatusUpdate(status);
  }

  void OnStatusUpdate (const buzz::Status &status) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_STATUS_UPDATE, new SendPresenceData(status));
    if (notify_)
      notify_->WakeupMainThread();
  }

  struct StatusErrorData : talk_base::MessageData {
    StatusErrorData(const buzz::XmlElement &stanza) : stanza_(stanza) {}
    buzz::XmlElement stanza_;
  };

  void OnStatusErrorW (const buzz::XmlElement &stanza) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnStatusError(stanza);
  }

  void OnStatusError (const buzz::XmlElement &stanza) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_STATUS_ERROR, new StatusErrorData(stanza));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void LoginW() {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    XmppSocket* socket = new XmppSocket(true);
    pump_->DoLogin(xcs_, socket, is_test_login_ ?  NULL : new XmppAuth());
    socket->SignalCloseEvent.connect(this,
        &LibjinglePlusWorker::OnXmppSocketClose);
  }

  void DisconnectW() {
    assert(talk_base::ThreadManager::CurrentThread() == worker_thread_);
    pump_->DoDisconnect();
  }

  void SendXmppMessageW(const buzz::XmppMessage &m) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::SendMessageTask * smt = new buzz::SendMessageTask(pump_.get()->client());
    smt->Send(m);
    smt->Start();
  }

  void SendXmppIqW(const buzz::Jid &to_jid, bool is_get,
                   const buzz::XmlElement *xml_element) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::IqTask *iq_task = new buzz::IqTask(pump_.get()->client(),
        is_get, to_jid, const_cast<buzz::XmlElement *>(xml_element));
    iq_task->SignalDone.connect(this, &LibjinglePlusWorker::OnIqComplete);
    iq_task->Start();
  }

 struct IqCompleteData : public talk_base::MessageData {
   IqCompleteData(bool success, const buzz::XmlElement *stanza) :
     success_(success), stanza_(*stanza) {}
   bool success_;
   buzz::XmlElement stanza_;
 };

  void OnIqCompleteW(bool success, const buzz::XmlElement& stanza) {
    assert(talk_base::ThreadManager::CurrentThread() != worker_thread_);
    if (notify_)
      notify_->OnIqDone(success, stanza);
  }

  void OnIqComplete(bool success, const buzz::XmlElement *stanza) {
    assert(talk_base::ThreadManager::CurrentThread() == worker_thread_);
    main_thread_->Post(this, MSG_IQ_COMPLETE,
       new IqCompleteData(success, stanza));
    if (notify_)
      notify_->WakeupMainThread();
  }

  void SendPresenceW(const buzz::Status & s) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::PresenceOutTask *pot = new buzz::PresenceOutTask(pump_.get()->client());
    pot->Send(s);
    pot->Start();
  }


  void SendDirectedMUCPresenceW(const buzz::Jid & j, const buzz::Status & s, 
			       const std::string &user_nick, const std::string &api_capability,
			       const std::string &api_message, const std::string &role) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::PresenceOutTask *pot = new buzz::PresenceOutTask(pump_.get()->client());
    pot->SendDirectedMUC(j,s,user_nick,api_capability,api_message, role);
    pot->Start();
  }
  
  void SendDirectedPresenceW(const buzz::Jid & j, const buzz::Status & s) {
    assert (talk_base::ThreadManager::CurrentThread() == worker_thread_);
    buzz::PresenceOutTask *pot = new buzz::PresenceOutTask(pump_.get()->client());
    pot->SendDirected(j,s);
    pot->Start();
  }

  void OnXmppSocketClose(int error) {
    notify_->OnSocketClose(error);
  }

 struct SendMessageData : public talk_base::MessageData {
   SendMessageData(const buzz::XmppMessage &m) : m_(m) {}
   buzz::XmppMessage m_;
  };

 struct SendIqData : public talk_base::MessageData {
   SendIqData(const buzz::Jid &jid, bool is_get, const buzz::XmlElement *m)
     : to_jid_(jid), is_get_(is_get), xml_element_(m) {}
   buzz::Jid to_jid_;
   bool is_get_;
   const buzz::XmlElement *xml_element_;
  };

 struct SendPresenceData : public talk_base::MessageData {
   SendPresenceData(const buzz::Status &s) : s_(s) {}
   buzz::Status s_;
  };  

 struct SendDirectedPresenceData : public talk_base::MessageData {
   SendDirectedPresenceData(const buzz::Jid &j, const buzz::Status &s) : j_(j), s_(s) {}
   buzz::Jid j_;
   buzz::Status s_;
 };

  struct SendDirectedMUCPresenceData : public talk_base::MessageData {
    SendDirectedMUCPresenceData(const buzz::Jid &j, const buzz::Status &s,
				const std::string &un, const std::string &ac,
				const std::string &am, const std::string &role)
      : j_(j), s_(s), un_(un), ac_(ac), am_(am), role_(role) {}
    buzz::Jid j_;
    buzz::Status s_;
    std::string un_;
    std::string ac_;
    std::string am_;
    std::string role_;
  };

  talk_base::scoped_ptr<talk_base::Win32SocketServer> ss_;
  talk_base::scoped_ptr<talk_base::Thread> main_thread_;
  talk_base::Thread *worker_thread_;

  LibjinglePlus *ljp_;
  LibjinglePlusNotify *notify_;
  buzz::XmppClientSettings xcs_;
  talk_base::PhysicalSocketServer pss_;

  talk_base::scoped_ptr<XmppPump> pump_;
  buzz::PresencePushTask * ppt_;
  buzz::ReceiveMessageTask * rmt_;
  buzz::RosterTask * rt_;

  bool is_test_login_;
};

LibjinglePlus::LibjinglePlus(LibjinglePlusNotify *notify)
{
  worker_ = new LibjinglePlusWorker(this, notify);
}

LibjinglePlus::~LibjinglePlus()
{
 delete worker_;
  worker_ = NULL;
}

void LibjinglePlus::Login(const std::string &jid,
		          const std::string &password,
		          const std::string &machine_address,
			  bool is_test, bool cookie_auth) {
  worker_->Login(jid, password, machine_address, is_test, cookie_auth);
}

void LibjinglePlus::SendPresence(const buzz::Status & s) {
  worker_->SendPresence(s);
}

void LibjinglePlus::SendDirectedPresence(const buzz::Jid & j, const buzz::Status & s) {
  worker_->SendDirectedPresence(j,s);
}

void LibjinglePlus::SendDirectedMUCPresence(const buzz::Jid & j,
    const buzz::Status & s, const std::string &user_nick,
    const std::string &api_capability, const std::string &api_message,
    const std::string &role) {
  worker_->SendDirectedMUCPresence(j,s,user_nick,api_capability,api_message,
      role);
}

void LibjinglePlus::SendXmppMessage(const buzz::XmppMessage & m) {
  worker_->SendXmppMessage(m);
}

void LibjinglePlus::SendXmppIq(const buzz::Jid &to_jid, bool is_get,
                               const buzz::XmlElement *iq_element) {
  worker_->SendXmppIq(to_jid, is_get, iq_element);
}

void LibjinglePlus::DoCallbacks() {
  worker_->DoCallbacks();
}
