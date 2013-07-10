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

#ifndef _moduleimpl_h_
#define _moduleimpl_h_

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/module.h"

namespace buzz {

//! This is the base implementation class for extension modules.
//! An engine is registered with the module and the module then hooks the
//! appropriate parts of the engine to implement that set of features.  It is
//! important to unregister modules before destructing the engine.
class XmppModuleImpl {
protected:
  XmppModuleImpl();
  virtual ~XmppModuleImpl();

  //! Register the engine with the module.  Only one engine can be associated
  //! with a module at a time.  This method will return an error if there is
  //! already an engine registered.
  XmppReturnStatus RegisterEngine(XmppEngine* engine);

  //! Gets the engine that this module is attached to.
  XmppEngine* engine();

  //! Process the given stanza.
  //! The module must return true if it has handled the stanza.
  //! A false return value causes the stanza to be passed on to
  //! the next registered handler.
  virtual bool HandleStanza(const XmlElement *) { return false; };

private:

  //! The ModuleSessionHelper nested class allows the Module
  //! to hook into and get stanzas and events from the engine.
  class ModuleStanzaHandler : public XmppStanzaHandler {
    friend class XmppModuleImpl;

    ModuleStanzaHandler(XmppModuleImpl* module) :
      module_(module) {
    }

    bool HandleStanza(const XmlElement* stanza) {
      return module_->HandleStanza(stanza);
    }

    XmppModuleImpl* module_;
  };

  friend class ModuleStanzaHandler;

  XmppEngine* engine_;
  ModuleStanzaHandler stanza_handler_;
};


// This macro will implement the XmppModule interface for a class
// that derives from both XmppModuleImpl and XmppModule
#define IMPLEMENT_XMPPMODULE \
  XmppReturnStatus RegisterEngine(XmppEngine* engine) { \
    return XmppModuleImpl::RegisterEngine(engine); \
  }

}

#endif
