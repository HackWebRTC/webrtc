/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_MANAGER_BASE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_MANAGER_BASE_H_

namespace webrtc {
class RWLockWrapper;

class ViEManagerBase
{
    friend class ViEManagerScopedBase;
    friend class ViEManagedItemScopedBase;
    friend class ViEManagerWriteScoped;
public:
    ViEManagerBase(void);
    ~ViEManagerBase(void);
private:
    void WriteLockManager();
    void ReleaseWriteLockManager();
    void ReadLockManager() const;
    void ReleaseLockManager() const;
    RWLockWrapper& _instanceRWLock;
};

class ViEManagerWriteScoped
{
public:
    ViEManagerWriteScoped(ViEManagerBase& vieManager);
    ~ViEManagerWriteScoped();
private:
    ViEManagerBase* _vieManager;
};

class ViEManagerScopedBase
{
    friend class ViEManagedItemScopedBase;
public:
    ViEManagerScopedBase(const ViEManagerBase& vieManager);
    ~ViEManagerScopedBase();
protected:
    const ViEManagerBase* _vieManager;
private:
    int _refCount;
};

class ViEManagedItemScopedBase
{
public:
    ViEManagedItemScopedBase(ViEManagerScopedBase& vieScopedManager);
    ~ViEManagedItemScopedBase();
protected:
    ViEManagerScopedBase& _vieScopedManager;
};
} // namespace webrtc
#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_MANAGER_BASE_H_
