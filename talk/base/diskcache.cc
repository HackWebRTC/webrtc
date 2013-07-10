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

#include <time.h>

#ifdef WIN32
#include "talk/base/win32.h"
#endif

#include "talk/base/common.h"
#include "talk/base/diskcache.h"
#include "talk/base/fileutils.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"

#ifdef _DEBUG
#define TRANSPARENT_CACHE_NAMES 1
#else  // !_DEBUG
#define TRANSPARENT_CACHE_NAMES 0
#endif  // !_DEBUG

namespace talk_base {

class DiskCache;

///////////////////////////////////////////////////////////////////////////////
// DiskCacheAdapter
///////////////////////////////////////////////////////////////////////////////

class DiskCacheAdapter : public StreamAdapterInterface {
public:
  DiskCacheAdapter(const DiskCache* cache, const std::string& id, size_t index,
                   StreamInterface* stream)
  : StreamAdapterInterface(stream), cache_(cache), id_(id), index_(index)
  { }
  virtual ~DiskCacheAdapter() {
    Close();
    cache_->ReleaseResource(id_, index_);
  }

private:
  const DiskCache* cache_;
  std::string id_;
  size_t index_;
};

///////////////////////////////////////////////////////////////////////////////
// DiskCache
///////////////////////////////////////////////////////////////////////////////

DiskCache::DiskCache() : max_cache_(0), total_size_(0), total_accessors_(0) {
}

DiskCache::~DiskCache() {
  ASSERT(0 == total_accessors_);
}

bool DiskCache::Initialize(const std::string& folder, size_t size) {
  if (!folder_.empty() || !Filesystem::CreateFolder(folder))
    return false;

  folder_ = folder;
  max_cache_ = size;
  ASSERT(0 == total_size_);

  if (!InitializeEntries())
    return false;

  return CheckLimit();
}

bool DiskCache::Purge() {
  if (folder_.empty())
    return false;

  if (total_accessors_ > 0) {
    LOG_F(LS_WARNING) << "Cache files open";
    return false;
  }

  if (!PurgeFiles())
    return false;

  map_.clear();
  return true;
}

bool DiskCache::LockResource(const std::string& id) {
  Entry* entry = GetOrCreateEntry(id, true);
  if (LS_LOCKED == entry->lock_state)
    return false;
  if ((LS_UNLOCKED == entry->lock_state) && (entry->accessors > 0))
    return false;
  if ((total_size_ > max_cache_) && !CheckLimit()) {
    LOG_F(LS_WARNING) << "Cache overfull";
    return false;
  }
  entry->lock_state = LS_LOCKED;
  return true;
}

StreamInterface* DiskCache::WriteResource(const std::string& id, size_t index) {
  Entry* entry = GetOrCreateEntry(id, false);
  if (LS_LOCKED != entry->lock_state)
    return NULL;

  size_t previous_size = 0;
  std::string filename(IdToFilename(id, index));
  FileStream::GetSize(filename, &previous_size);
  ASSERT(previous_size <= entry->size);
  if (previous_size > entry->size) {
    previous_size = entry->size;
  }

  scoped_ptr<FileStream> file(new FileStream);
  if (!file->Open(filename, "wb", NULL)) {
    LOG_F(LS_ERROR) << "Couldn't create cache file";
    return NULL;
  }

  entry->streams = stdmax(entry->streams, index + 1);
  entry->size -= previous_size;
  total_size_ -= previous_size;

  entry->accessors += 1;
  total_accessors_ += 1;
  return new DiskCacheAdapter(this, id, index, file.release());
}

bool DiskCache::UnlockResource(const std::string& id) {
  Entry* entry = GetOrCreateEntry(id, false);
  if (LS_LOCKED != entry->lock_state)
    return false;

  if (entry->accessors > 0) {
    entry->lock_state = LS_UNLOCKING;
  } else {
    entry->lock_state = LS_UNLOCKED;
    entry->last_modified = time(0);
    CheckLimit();
  }
  return true;
}

StreamInterface* DiskCache::ReadResource(const std::string& id,
                                         size_t index) const {
  const Entry* entry = GetEntry(id);
  if (LS_UNLOCKED != entry->lock_state)
    return NULL;
  if (index >= entry->streams)
    return NULL;

  scoped_ptr<FileStream> file(new FileStream);
  if (!file->Open(IdToFilename(id, index), "rb", NULL))
    return NULL;

  entry->accessors += 1;
  total_accessors_ += 1;
  return new DiskCacheAdapter(this, id, index, file.release());
}

bool DiskCache::HasResource(const std::string& id) const {
  const Entry* entry = GetEntry(id);
  return (NULL != entry) && (entry->streams > 0);
}

bool DiskCache::HasResourceStream(const std::string& id, size_t index) const {
  const Entry* entry = GetEntry(id);
  if ((NULL == entry) || (index >= entry->streams))
    return false;

  std::string filename = IdToFilename(id, index);

  return FileExists(filename);
}

bool DiskCache::DeleteResource(const std::string& id) {
  Entry* entry = GetOrCreateEntry(id, false);
  if (!entry)
    return true;

  if ((LS_UNLOCKED != entry->lock_state) || (entry->accessors > 0))
    return false;

  bool success = true;
  for (size_t index = 0; index < entry->streams; ++index) {
    std::string filename = IdToFilename(id, index);

    if (!FileExists(filename))
      continue;

    if (!DeleteFile(filename)) {
      LOG_F(LS_ERROR) << "Couldn't remove cache file: " << filename;
      success = false;
    }
  }

  total_size_ -= entry->size;
  map_.erase(id);
  return success;
}

bool DiskCache::CheckLimit() {
#ifdef _DEBUG
  // Temporary check to make sure everything is working correctly.
  size_t cache_size = 0;
  for (EntryMap::iterator it = map_.begin(); it != map_.end(); ++it) {
    cache_size += it->second.size;
  }
  ASSERT(cache_size == total_size_);
#endif  // _DEBUG

  // TODO: Replace this with a non-brain-dead algorithm for clearing out the
  // oldest resources... something that isn't O(n^2)
  while (total_size_ > max_cache_) {
    EntryMap::iterator oldest = map_.end();
    for (EntryMap::iterator it = map_.begin(); it != map_.end(); ++it) {
      if ((LS_UNLOCKED != it->second.lock_state) || (it->second.accessors > 0))
        continue;
      oldest = it;
      break;
    }
    if (oldest == map_.end()) {
      LOG_F(LS_WARNING) << "All resources are locked!";
      return false;
    }
    for (EntryMap::iterator it = oldest++; it != map_.end(); ++it) {
      if (it->second.last_modified < oldest->second.last_modified) {
        oldest = it;
      }
    }
    if (!DeleteResource(oldest->first)) {
      LOG_F(LS_ERROR) << "Couldn't delete from cache!";
      return false;
    }
  }
  return true;
}

std::string DiskCache::IdToFilename(const std::string& id, size_t index) const {
#ifdef TRANSPARENT_CACHE_NAMES
  // This escapes colons and other filesystem characters, so the user can't open
  // special devices (like "COM1:"), or access other directories.
  size_t buffer_size = id.length()*3 + 1;
  char* buffer = new char[buffer_size];
  encode(buffer, buffer_size, id.data(), id.length(),
         unsafe_filename_characters(), '%');
  // TODO: ASSERT(strlen(buffer) < FileSystem::MaxBasenameLength());
#else  // !TRANSPARENT_CACHE_NAMES
  // We might want to just use a hash of the filename at some point, both for
  // obfuscation, and to avoid both filename length and escaping issues.
  ASSERT(false);
#endif  // !TRANSPARENT_CACHE_NAMES

  char extension[32];
  sprintfn(extension, ARRAY_SIZE(extension), ".%u", index);

  Pathname pathname;
  pathname.SetFolder(folder_);
  pathname.SetBasename(buffer);
  pathname.SetExtension(extension);

#ifdef TRANSPARENT_CACHE_NAMES
  delete [] buffer;
#endif  // TRANSPARENT_CACHE_NAMES

  return pathname.pathname();
}

bool DiskCache::FilenameToId(const std::string& filename, std::string* id,
                             size_t* index) const {
  Pathname pathname(filename);
  unsigned tempdex;
  if (1 != sscanf(pathname.extension().c_str(), ".%u", &tempdex))
    return false;

  *index = static_cast<size_t>(tempdex);

  size_t buffer_size = pathname.basename().length() + 1;
  char* buffer = new char[buffer_size];
  decode(buffer, buffer_size, pathname.basename().data(),
         pathname.basename().length(), '%');
  id->assign(buffer);
  delete [] buffer;
  return true;
}

DiskCache::Entry* DiskCache::GetOrCreateEntry(const std::string& id,
                                              bool create) {
  EntryMap::iterator it = map_.find(id);
  if (it != map_.end())
    return &it->second;
  if (!create)
    return NULL;
  Entry e;
  e.lock_state = LS_UNLOCKED;
  e.accessors = 0;
  e.size = 0;
  e.streams = 0;
  e.last_modified = time(0);
  it = map_.insert(EntryMap::value_type(id, e)).first;
  return &it->second;
}

void DiskCache::ReleaseResource(const std::string& id, size_t index) const {
  const Entry* entry = GetEntry(id);
  if (!entry) {
    LOG_F(LS_WARNING) << "Missing cache entry";
    ASSERT(false);
    return;
  }

  entry->accessors -= 1;
  total_accessors_ -= 1;

  if (LS_UNLOCKED != entry->lock_state) {
    // This is safe, because locked resources only issue WriteResource, which
    // is non-const.  Think about a better way to handle it.
    DiskCache* this2 = const_cast<DiskCache*>(this);
    Entry* entry2 = this2->GetOrCreateEntry(id, false);

    size_t new_size = 0;
    std::string filename(IdToFilename(id, index));
    FileStream::GetSize(filename, &new_size);
    entry2->size += new_size;
    this2->total_size_ += new_size;

    if ((LS_UNLOCKING == entry->lock_state) && (0 == entry->accessors)) {
      entry2->last_modified = time(0);
      entry2->lock_state = LS_UNLOCKED;
      this2->CheckLimit();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace talk_base
