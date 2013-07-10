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

#ifndef TALK_BASE_DISKCACHE_H__
#define TALK_BASE_DISKCACHE_H__

#include <map>
#include <string>

#ifdef WIN32
#undef UnlockResource
#endif  // WIN32

namespace talk_base {

class StreamInterface;

///////////////////////////////////////////////////////////////////////////////
// DiskCache - An LRU cache of streams, stored on disk.
//
// Streams are identified by a unique resource id.  Multiple streams can be
// associated with each resource id, distinguished by an index.  When old
// resources are flushed from the cache, all streams associated with those
// resources are removed together.
// DiskCache is designed to persist across executions of the program.  It is
// safe for use from an arbitrary number of users on a single thread, but not
// from multiple threads or other processes.
///////////////////////////////////////////////////////////////////////////////

class DiskCache {
public:
  DiskCache();
  virtual ~DiskCache();

  bool Initialize(const std::string& folder, size_t size);
  bool Purge();

  bool LockResource(const std::string& id);
  StreamInterface* WriteResource(const std::string& id, size_t index);
  bool UnlockResource(const std::string& id);

  StreamInterface* ReadResource(const std::string& id, size_t index) const;

  bool HasResource(const std::string& id) const;
  bool HasResourceStream(const std::string& id, size_t index) const;
  bool DeleteResource(const std::string& id);

 protected:
  virtual bool InitializeEntries() = 0;
  virtual bool PurgeFiles() = 0;

  virtual bool FileExists(const std::string& filename) const = 0;
  virtual bool DeleteFile(const std::string& filename) const = 0;

  enum LockState { LS_UNLOCKED, LS_LOCKED, LS_UNLOCKING };
  struct Entry {
    LockState lock_state;
    mutable size_t accessors;
    size_t size;
    size_t streams;
    time_t last_modified;
  };
  typedef std::map<std::string, Entry> EntryMap;
  friend class DiskCacheAdapter;

  bool CheckLimit();

  std::string IdToFilename(const std::string& id, size_t index) const;
  bool FilenameToId(const std::string& filename, std::string* id,
                    size_t* index) const;

  const Entry* GetEntry(const std::string& id) const {
    return const_cast<DiskCache*>(this)->GetOrCreateEntry(id, false);
  }
  Entry* GetOrCreateEntry(const std::string& id, bool create);

  void ReleaseResource(const std::string& id, size_t index) const;

  std::string folder_;
  size_t max_cache_, total_size_;
  EntryMap map_;
  mutable size_t total_accessors_;
};

///////////////////////////////////////////////////////////////////////////////
// CacheLock - Automatically manage locking and unlocking, with optional
// rollback semantics
///////////////////////////////////////////////////////////////////////////////

class CacheLock {
public:
  CacheLock(DiskCache* cache, const std::string& id, bool rollback = false)
  : cache_(cache), id_(id), rollback_(rollback)
  {
    locked_ = cache_->LockResource(id_);
  }
  ~CacheLock() {
    if (locked_) {
      cache_->UnlockResource(id_);
      if (rollback_) {
        cache_->DeleteResource(id_);
      }
    }
  }
  bool IsLocked() const { return locked_; }
  void Commit() { rollback_ = false; }

private:
  DiskCache* cache_;
  std::string id_;
  bool rollback_, locked_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif // TALK_BASE_DISKCACHE_H__
