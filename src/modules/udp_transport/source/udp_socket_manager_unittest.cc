/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Tests for the UdpSocketManager interface.
// Note: This tests UdpSocketManager together with UdpSocketWrapper,
// due to the way the code is full of static-casts to the platform dependent
// subtypes.
// It also uses the static UdpSocketManager object.
// The most important property of these tests is that they do not leak memory.

#include "udp_socket_wrapper.h"
#include "udp_socket_manager_wrapper.h"
#include "gtest/gtest.h"

TEST(UdpSocketManager, CreateCallsInitAndDoesNotLeakMemory) {
  WebRtc_Word32 id = 42;
  WebRtc_UWord8 threads = 1;
  webrtc::UdpSocketManager* mgr = webrtc::UdpSocketManager::Create(id, threads);
  // Create is supposed to have called init on the object.
  EXPECT_EQ(false, mgr->Init(id, threads))
      << "Init should return false since Create is supposed to call it.";
  webrtc::UdpSocketManager::Return();
}

// Creates a socket and adds it to the socket manager, and then removes it
// before destroying the socket manager.
TEST(UdpSocketManager, AddAndRemoveSocketDoesNotLeakMemory) {
  WebRtc_Word32 id = 42;
  WebRtc_UWord8 threads = 1;
  webrtc::UdpSocketManager* mgr = webrtc::UdpSocketManager::Create(id, threads);
  webrtc::UdpSocketWrapper* socket
       = webrtc::UdpSocketWrapper::CreateSocket(id,
                                                mgr,
                                                NULL,  // CallbackObj
                                                NULL,  // IncomingSocketCallback
                                                false,  // ipV6Enable
                                                false);  // disableGQOS
  // The constructor will do AddSocket on the manager.
  EXPECT_EQ(true, mgr->RemoveSocket(socket));
  webrtc::UdpSocketManager::Return();
}

// Creates a socket and add it to the socket manager, but does not remove it
// before destroying the socket manager.
// This should also destroy the socket.
TEST(UdpSocketManager, DISABLED_UnremovedSocketsGetCollectedAtManagerDeletion) {
  WebRtc_Word32 id = 42;
  WebRtc_UWord8 threads = 1;
  webrtc::UdpSocketManager* mgr = webrtc::UdpSocketManager::Create(id, threads);
  webrtc::UdpSocketWrapper* unused_socket
       = webrtc::UdpSocketWrapper::CreateSocket(id,
                                                mgr,
                                                NULL,  // CallbackObj
                                                NULL,  // IncomingSocketCallback
                                                false,  // ipV6Enable
                                                false);  // disableGQOS
  // The constructor will do AddSocket on the manager.
  unused_socket = NULL;
  webrtc::UdpSocketManager::Return();
}
