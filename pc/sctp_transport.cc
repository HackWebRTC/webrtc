/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_transport.h"

#include <utility>

namespace webrtc {

SctpTransport::SctpTransport(
    std::unique_ptr<cricket::SctpTransportInternal> internal)
    : owner_thread_(rtc::Thread::Current()),
      info_(SctpTransportState::kNew),
      internal_sctp_transport_(std::move(internal)) {
  RTC_DCHECK(internal_sctp_transport_.get());
  internal_sctp_transport_->SignalReadyToSendData.connect(
      this, &SctpTransport::OnInternalReadyToSendData);
  // TODO(https://bugs.webrtc.org/10360): Add handlers for transport closing.

  if (dtls_transport_) {
    UpdateInformation(SctpTransportState::kConnecting);
  } else {
    UpdateInformation(SctpTransportState::kNew);
  }
}

SctpTransport::~SctpTransport() {
  // We depend on the network thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_sctp_transport_);
}

SctpTransportInformation SctpTransport::Information() const {
  rtc::CritScope scope(&lock_);
  return info_;
}

void SctpTransport::RegisterObserver(SctpTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  RTC_DCHECK(!observer_);
  observer_ = observer;
}

void SctpTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

rtc::scoped_refptr<DtlsTransportInterface> SctpTransport::dtls_transport()
    const {
  RTC_DCHECK_RUN_ON(owner_thread_);
  return dtls_transport_;
}

// Internal functions
void SctpTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal());
  {
    rtc::CritScope scope(&lock_);
    // Note that we delete internal_sctp_transport_, but
    // only drop the reference to dtls_transport_.
    dtls_transport_ = nullptr;
    internal_sctp_transport_ = nullptr;
  }
  UpdateInformation(SctpTransportState::kClosed);
}

void SctpTransport::SetDtlsTransport(
    rtc::scoped_refptr<DtlsTransport> transport) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  rtc::CritScope scope(&lock_);
  dtls_transport_ = transport;
  if (internal_sctp_transport_) {
    if (transport) {
      internal_sctp_transport_->SetDtlsTransport(transport->internal());
      if (info_.state() == SctpTransportState::kNew) {
        UpdateInformation(SctpTransportState::kConnecting);
      }
    } else {
      internal_sctp_transport_->SetDtlsTransport(nullptr);
    }
  }
}

void SctpTransport::UpdateInformation(SctpTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  bool must_send_update;
  SctpTransportInformation info_copy(SctpTransportState::kNew);
  {
    rtc::CritScope scope(&lock_);
    must_send_update = (state != info_.state());
    // TODO(https://bugs.webrtc.org/10358): Update max message size and
    // max channels from internal SCTP transport when available.
    info_ = SctpTransportInformation(
        state, dtls_transport_, info_.MaxMessageSize(), info_.MaxChannels());
    if (observer_ && must_send_update) {
      info_copy = info_;
    }
  }
  // We call the observer without holding the lock.
  if (observer_ && must_send_update) {
    observer_->OnStateChange(info_copy);
  }
}

void SctpTransport::OnInternalReadyToSendData() {
  UpdateInformation(SctpTransportState::kConnected);
}

}  // namespace webrtc
