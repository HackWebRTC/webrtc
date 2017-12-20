/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contain convenience functions and classes for JNI.
// Before using any of the methods, InitGlobalJniVariables must be called.

#ifndef SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_
#define SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_

#include <jni.h>
#include <map>
#include <string>
#include <vector>

#include "api/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/refcount.h"
#include "rtc_base/thread_checker.h"
#include "sdk/android/src/jni/scoped_java_ref.h"

// Abort the process if |jni| has a Java exception pending.
// This macros uses the comma operator to execute ExceptionDescribe
// and ExceptionClear ignoring their return values and sending ""
// to the error stream.
#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

// Convenience macro defining JNI-accessible methods in the org.webrtc package.
// Eliminates unnecessary boilerplate and line-wraps, reducing visual clutter.
#define JNI_FUNCTION_DECLARATION(rettype, name, ...) \
  extern "C" JNIEXPORT rettype JNICALL Java_org_webrtc_##name(__VA_ARGS__)

namespace webrtc {
namespace jni {

jint InitGlobalJniVariables(JavaVM *jvm);

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv();

JavaVM *GetJVM();

// Return a |JNIEnv*| usable on this thread.  Attaches to |g_jvm| if necessary.
JNIEnv* AttachCurrentThreadIfNeeded();

// Return a |jlong| that will correctly convert back to |ptr|.  This is needed
// because the alternative (of silently passing a 32-bit pointer to a vararg
// function expecting a 64-bit param) picks up garbage in the high 32 bits.
jlong jlongFromPointer(void* ptr);

// Returns true if |obj| == null in Java.
bool IsNull(JNIEnv* jni, const JavaRef<jobject>& obj);

ScopedJavaLocalRef<jobject> NewDirectByteBuffer(JNIEnv* env,
                                                void* address,
                                                jlong capacity);
// Given a (UTF-16) jstring return a new UTF-8 native string.
std::string JavaToStdString(JNIEnv* jni, const JavaRef<jstring>& j_string);

// Deprecated. Use scoped jobjects instead.
inline std::string JavaToStdString(JNIEnv* jni, jstring j_string) {
  return JavaToStdString(jni, JavaParamRef<jstring>(j_string));
}

// Given a List of (UTF-16) jstrings
// return a new vector of UTF-8 native strings.
std::vector<std::string> JavaToStdVectorStrings(JNIEnv* jni,
                                                const JavaRef<jobject>& list);

rtc::Optional<int32_t> JavaToNativeOptionalInt(JNIEnv* jni,
                                               const JavaRef<jobject>& integer);
rtc::Optional<bool> JavaToNativeOptionalBool(JNIEnv* jni,
                                             const JavaRef<jobject>& boolean);
int64_t JavaToNativeLong(JNIEnv* env, const JavaRef<jobject>& j_long);

ScopedJavaLocalRef<jobject> NativeToJavaBoolean(JNIEnv* env, bool b);
ScopedJavaLocalRef<jobject> NativeToJavaInteger(JNIEnv* jni, int32_t i);
ScopedJavaLocalRef<jobject> NativeToJavaLong(JNIEnv* env, int64_t u);
ScopedJavaLocalRef<jobject> NativeToJavaDouble(JNIEnv* env, double d);
ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* jni, const char* str);
ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* jni,
                                               const std::string& str);
ScopedJavaLocalRef<jobject> NativeToJavaInteger(
    JNIEnv* jni,
    const rtc::Optional<int32_t>& optional_int);

// Parses Map<String, String> to std::map<std::string, std::string>.
std::map<std::string, std::string> JavaToStdMapStrings(
    JNIEnv* jni,
    const JavaRef<jobject>& j_map);

// Deprecated. Use scoped jobjects instead.
inline std::map<std::string, std::string> JavaToStdMapStrings(JNIEnv* jni,
                                                              jobject j_map) {
  return JavaToStdMapStrings(jni, JavaParamRef<jobject>(j_map));
}

// Returns the name of a Java enum.
std::string GetJavaEnumName(JNIEnv* jni, const JavaRef<jobject>& j_enum);

jobject NewGlobalRef(JNIEnv* jni, jobject o);

void DeleteGlobalRef(JNIEnv* jni, jobject o);

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
class ScopedLocalRefFrame {
 public:
  explicit ScopedLocalRefFrame(JNIEnv* jni);
  ~ScopedLocalRefFrame();

 private:
  JNIEnv* jni_;
};

// Provides a convenient way to iterate over a Java Iterable using the
// C++ range-for loop.
// E.g. for (jobject value : Iterable(jni, j_iterable)) { ... }
// Note: Since Java iterators cannot be duplicated, the iterator class is not
// copyable to prevent creating multiple C++ iterators that refer to the same
// Java iterator.
class Iterable {
 public:
  Iterable(JNIEnv* jni, const JavaRef<jobject>& iterable);
  ~Iterable();

  class Iterator {
   public:
    // Creates an iterator representing the end of any collection.
    Iterator();
    // Creates an iterator pointing to the beginning of the specified
    // collection.
    Iterator(JNIEnv* jni, const JavaRef<jobject>& iterable);

    // Move constructor - necessary to be able to return iterator types from
    // functions.
    Iterator(Iterator&& other);

    ~Iterator();

    // Move assignment should not be used.
    Iterator& operator=(Iterator&&) = delete;

    // Advances the iterator one step.
    Iterator& operator++();

    // Removes the element the iterator is pointing to. Must still advance the
    // iterator afterwards.
    void Remove();

    // Provides a way to compare the iterator with itself and with the end
    // iterator.
    // Note: all other comparison results are undefined, just like for C++ input
    // iterators.
    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other) { return !(*this == other); }
    ScopedJavaLocalRef<jobject>& operator*();

   private:
    bool AtEnd() const;

    JNIEnv* jni_ = nullptr;
    ScopedJavaLocalRef<jobject> iterator_;
    ScopedJavaLocalRef<jobject> value_;
    rtc::ThreadChecker thread_checker_;

    RTC_DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  Iterable::Iterator begin() { return Iterable::Iterator(jni_, iterable_); }
  Iterable::Iterator end() { return Iterable::Iterator(); }

 private:
  JNIEnv* jni_;
  ScopedJavaLocalRef<jobject> iterable_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Iterable);
};

// Helper function for converting std::vector<T> into a Java array.
template <typename T, typename Convert>
ScopedJavaLocalRef<jobjectArray> NativeToJavaObjectArray(
    JNIEnv* env,
    const std::vector<T>& container,
    jclass clazz,
    Convert convert) {
  ScopedJavaLocalRef<jobjectArray> j_container(
      env, env->NewObjectArray(container.size(), clazz, nullptr));
  int i = 0;
  for (const T& element : container) {
    env->SetObjectArrayElement(j_container.obj(), i,
                               convert(env, element).obj());
    ++i;
  }
  return j_container;
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaIntegerArray(
    JNIEnv* env,
    const std::vector<int32_t>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaLongArray(
    JNIEnv* env,
    const std::vector<int64_t>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaDoubleArray(
    JNIEnv* env,
    const std::vector<double>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaStringArray(
    JNIEnv* env,
    const std::vector<std::string>& container);

template <typename T, typename Convert>
std::vector<T> JavaToNativeVector(JNIEnv* env,
                                  const JavaRef<jobjectArray>& j_container,
                                  Convert convert) {
  std::vector<T> container;
  const size_t size = env->GetArrayLength(j_container.obj());
  container.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    container.emplace_back(convert(
        env, ScopedJavaLocalRef<jobject>(
                 env, env->GetObjectArrayElement(j_container.obj(), i))));
  }
  CHECK_EXCEPTION(env) << "Error during JavaToNativeVector";
  return container;
}

// This is a helper class for NativeToJavaList(). Use that function instead of
// using this class directly.
class JavaListBuilder {
 public:
  explicit JavaListBuilder(JNIEnv* env);
  ~JavaListBuilder();
  void add(const JavaRef<jobject>& element);
  ScopedJavaLocalRef<jobject> java_list() { return j_list_; }

 private:
  JNIEnv* env_;
  ScopedJavaLocalRef<jobject> j_list_;
};

template <typename C, typename Convert>
ScopedJavaLocalRef<jobject> NativeToJavaList(JNIEnv* env,
                                             const C& container,
                                             Convert convert) {
  JavaListBuilder builder(env);
  for (const auto& e : container)
    builder.add(convert(env, e));
  return builder.java_list();
}

// This is a helper class for NativeToJavaMap(). Use that function instead of
// using this class directly.
class JavaMapBuilder {
 public:
  explicit JavaMapBuilder(JNIEnv* env);
  ~JavaMapBuilder();
  void put(const JavaRef<jobject>& key, const JavaRef<jobject>& value);
  ScopedJavaLocalRef<jobject> GetJavaMap() { return j_map_; }

 private:
  JNIEnv* env_;
  ScopedJavaLocalRef<jobject> j_map_;
};

template <typename C, typename Convert>
ScopedJavaLocalRef<jobject> NativeToJavaMap(JNIEnv* env,
                                            const C& container,
                                            Convert convert) {
  JavaMapBuilder builder(env);
  for (const auto& e : container) {
    ScopedLocalRefFrame local_ref_frame(env);
    const auto key_value_pair = convert(env, e);
    builder.put(key_value_pair.first, key_value_pair.second);
  }
  return builder.GetJavaMap();
}

}  // namespace jni
}  // namespace webrtc

// TODO(magjed): Remove once external clients are updated.
namespace webrtc_jni {

using webrtc::jni::AttachCurrentThreadIfNeeded;
using webrtc::jni::InitGlobalJniVariables;

}  // namespace webrtc_jni

#endif  // SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_
