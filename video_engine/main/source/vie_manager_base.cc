/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_manager_base.h"
#include "rw_lock_wrapper.h"
#include "assert.h"

namespace webrtc {

ViEManagerBase::ViEManagerBase() :
    _instanceRWLock(*RWLockWrapper::CreateRWLock())
{
}
ViEManagerBase::~ViEManagerBase()
{
    delete &_instanceRWLock;
}

// ----------------------------------------------------------------------------
// ReadLockManager
//
// Lock count increase. Used  by ViEManagerScopedBase
// ----------------------------------------------------------------------------
void ViEManagerBase::ReadLockManager() const
{
    _instanceRWLock.AcquireLockShared();
}

// ----------------------------------------------------------------------------
// ReleaseLockManager
//
// Releases the lock count.
// ----------------------------------------------------------------------------
void ViEManagerBase::ReleaseLockManager() const
{
    _instanceRWLock.ReleaseLockShared();
}

// ----------------------------------------------------------------------------
// WriteLockManager
//
// Lock count increase. Used  by ViEManagerWriteScoped
// ----------------------------------------------------------------------------
void ViEManagerBase::WriteLockManager()
{
    _instanceRWLock.AcquireLockExclusive();
}

// ----------------------------------------------------------------------------
// ReleaseLockManager
//
// Releases the lock count.
// ----------------------------------------------------------------------------
void ViEManagerBase::ReleaseWriteLockManager()
{
    _instanceRWLock.ReleaseLockExclusive();
}

// ----------------------------------------------------------------------------
// ViEManagerScopedBase
//
// ----------------------------------------------------------------------------
ViEManagerScopedBase::ViEManagerScopedBase(const ViEManagerBase& ViEManagerBase) :
    _vieManager(&ViEManagerBase), _refCount(0)
{
    _vieManager->ReadLockManager();
}

ViEManagerScopedBase::~ViEManagerScopedBase()
{
    assert(_refCount==0);
    _vieManager->ReleaseLockManager();
}

// ----------------------------------------------------------------------------
///
// ViEManagerWriteScoped
//
// ----------------------------------------------------------------------------
ViEManagerWriteScoped::ViEManagerWriteScoped(ViEManagerBase& vieManager) :
    _vieManager(&vieManager)
{
    _vieManager->WriteLockManager();
}

ViEManagerWriteScoped::~ViEManagerWriteScoped()
{
    _vieManager->ReleaseWriteLockManager();
}

// ----------------------------------------------------------------------------
// ViEManagedItemScopedBase
//
// ----------------------------------------------------------------------------
ViEManagedItemScopedBase::ViEManagedItemScopedBase(
                                                   ViEManagerScopedBase& vieScopedManager) :
    _vieScopedManager(vieScopedManager)
{
    _vieScopedManager._refCount++;
}

ViEManagedItemScopedBase::~ViEManagedItemScopedBase()
{
    _vieScopedManager._refCount--;
}
} // namespace webrtc
