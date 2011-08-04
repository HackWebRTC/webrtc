#ifndef TALK_APP_WEBRTC_NOTIFIER_IMPL_H_
#define TALK_APP_WEBRTC_NOTIFIER_IMPL_H_

// Implement a template version of a notifier.
// TODO - allow multiple observers.
//#include <list>

#include "talk/app/webrtc/stream_dev.h"

namespace webrtc {
template <class T>
class NotifierImpl : public T{
public:
  NotifierImpl(){
  }

  virtual void RegisterObserver(Observer* observer) {
    observer_ = observer;
  }

  virtual void UnregisterObserver(Observer*) {
    observer_ = NULL;
  }

 void FireOnChanged() {
   if(observer_)
     observer_->OnChanged();
 }

protected:
  Observer* observer_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_REF_COUNT_H_
