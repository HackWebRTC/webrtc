#ifndef TALK_APP_WEBRTC_REF_COUNT_H_
#define TALK_APP_WEBRTC_REF_COUNT_H_

#include <cstring>

// Reference count interface.
class RefCount {
 public:
  virtual size_t AddRef() = 0;
  virtual size_t Release() = 0;
};

template <class T>
class RefCountImpl : public T {
public:
  RefCountImpl() : ref_count_(0) {
  }

  template<typename P>
  RefCountImpl(P p) : ref_count_(0), T(p) {
  }

  template<typename P1, typename P2>
  RefCountImpl(P1 p1, P2 p2) : ref_count_(0), T(p1, p2) {
  }

  virtual size_t AddRef() {
    ++ref_count_;
    return ref_count_;
  }

  virtual size_t Release() {
    size_t ret = --ref_count_;
    if(!ref_count_) {
      delete this;
    }
    return ret;
  }
protected:
  size_t ref_count_;
};

#endif  // TALK_APP_WEBRTC_REF_COUNT_H_
