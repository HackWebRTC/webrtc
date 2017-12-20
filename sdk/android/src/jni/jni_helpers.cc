/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/src/jni/jni_helpers.h"

#include <asm/unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

#include "sdk/android/generated_base_jni/jni/JniHelper_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/ArrayList_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Boolean_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Double_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Enum_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Integer_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Iterable_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Iterator_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/LinkedHashMap_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Long_jni.h"
#include "sdk/android/generated_external_classes_jni/jni/Map_jni.h"

namespace webrtc {
namespace jni {

static JavaVM* g_jvm = nullptr;

static pthread_once_t g_jni_ptr_once = PTHREAD_ONCE_INIT;

// Key for per-thread JNIEnv* data.  Non-NULL in threads attached to |g_jvm| by
// AttachCurrentThreadIfNeeded(), NULL in unattached threads and threads that
// were attached by the JVM because of a Java->native call.
static pthread_key_t g_jni_ptr;

JavaVM *GetJVM() {
  RTC_CHECK(g_jvm) << "JNI_OnLoad failed to run?";
  return g_jvm;
}

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv() {
  void* env = nullptr;
  jint status = g_jvm->GetEnv(&env, JNI_VERSION_1_6);
  RTC_CHECK(((env != nullptr) && (status == JNI_OK)) ||
            ((env == nullptr) && (status == JNI_EDETACHED)))
      << "Unexpected GetEnv return: " << status << ":" << env;
  return reinterpret_cast<JNIEnv*>(env);
}

static void ThreadDestructor(void* prev_jni_ptr) {
  // This function only runs on threads where |g_jni_ptr| is non-NULL, meaning
  // we were responsible for originally attaching the thread, so are responsible
  // for detaching it now.  However, because some JVM implementations (notably
  // Oracle's http://goo.gl/eHApYT) also use the pthread_key_create mechanism,
  // the JVMs accounting info for this thread may already be wiped out by the
  // time this is called. Thus it may appear we are already detached even though
  // it was our responsibility to detach!  Oh well.
  if (!GetEnv())
    return;

  RTC_CHECK(GetEnv() == prev_jni_ptr)
      << "Detaching from another thread: " << prev_jni_ptr << ":" << GetEnv();
  jint status = g_jvm->DetachCurrentThread();
  RTC_CHECK(status == JNI_OK) << "Failed to detach thread: " << status;
  RTC_CHECK(!GetEnv()) << "Detaching was a successful no-op???";
}

static void CreateJNIPtrKey() {
  RTC_CHECK(!pthread_key_create(&g_jni_ptr, &ThreadDestructor))
      << "pthread_key_create";
}

jint InitGlobalJniVariables(JavaVM *jvm) {
  RTC_CHECK(!g_jvm) << "InitGlobalJniVariables!";
  g_jvm = jvm;
  RTC_CHECK(g_jvm) << "InitGlobalJniVariables handed NULL?";

  RTC_CHECK(!pthread_once(&g_jni_ptr_once, &CreateJNIPtrKey)) << "pthread_once";

  JNIEnv* jni = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6) != JNI_OK)
    return -1;

  return JNI_VERSION_1_6;
}

// Return thread ID as a string.
static std::string GetThreadId() {
  char buf[21];  // Big enough to hold a kuint64max plus terminating NULL.
  RTC_CHECK_LT(snprintf(buf, sizeof(buf), "%ld",
                        static_cast<long>(syscall(__NR_gettid))),
               sizeof(buf))
      << "Thread id is bigger than uint64??";
  return std::string(buf);
}

// Return the current thread's name.
static std::string GetThreadName() {
  char name[17] = {0};
  if (prctl(PR_GET_NAME, name) != 0)
    return std::string("<noname>");
  return std::string(name);
}

// Return a |JNIEnv*| usable on this thread.  Attaches to |g_jvm| if necessary.
JNIEnv* AttachCurrentThreadIfNeeded() {
  JNIEnv* jni = GetEnv();
  if (jni)
    return jni;
  RTC_CHECK(!pthread_getspecific(g_jni_ptr))
      << "TLS has a JNIEnv* but not attached?";

  std::string name(GetThreadName() + " - " + GetThreadId());
  JavaVMAttachArgs args;
  args.version = JNI_VERSION_1_6;
  args.name = &name[0];
  args.group = nullptr;
  // Deal with difference in signatures between Oracle's jni.h and Android's.
#ifdef _JAVASOFT_JNI_H_  // Oracle's jni.h violates the JNI spec!
  void* env = nullptr;
#else
  JNIEnv* env = nullptr;
#endif
  RTC_CHECK(!g_jvm->AttachCurrentThread(&env, &args))
      << "Failed to attach thread";
  RTC_CHECK(env) << "AttachCurrentThread handed back NULL!";
  jni = reinterpret_cast<JNIEnv*>(env);
  RTC_CHECK(!pthread_setspecific(g_jni_ptr, jni)) << "pthread_setspecific";
  return jni;
}

// Return a |jlong| that will correctly convert back to |ptr|.  This is needed
// because the alternative (of silently passing a 32-bit pointer to a vararg
// function expecting a 64-bit param) picks up garbage in the high 32 bits.
jlong jlongFromPointer(void* ptr) {
  static_assert(sizeof(intptr_t) <= sizeof(jlong),
                "Time to rethink the use of jlongs");
  // Going through intptr_t to be obvious about the definedness of the
  // conversion from pointer to integral type.  intptr_t to jlong is a standard
  // widening by the static_assert above.
  jlong ret = reinterpret_cast<intptr_t>(ptr);
  RTC_DCHECK(reinterpret_cast<void*>(ret) == ptr);
  return ret;
}

bool IsNull(JNIEnv* jni, const JavaRef<jobject>& obj) {
  return jni->IsSameObject(obj.obj(), nullptr);
}

ScopedJavaLocalRef<jobject> NewDirectByteBuffer(JNIEnv* env,
                                                void* address,
                                                jlong capacity) {
  ScopedJavaLocalRef<jobject> buffer(
      env, env->NewDirectByteBuffer(address, capacity));
  CHECK_EXCEPTION(env) << "error NewDirectByteBuffer";
  return buffer;
}

// Given a jstring, reinterprets it to a new native string.
std::string JavaToStdString(JNIEnv* jni, const JavaRef<jstring>& j_string) {
  const ScopedJavaLocalRef<jbyteArray> j_byte_array =
      Java_JniHelper_getStringBytes(jni, j_string);

  const size_t len = jni->GetArrayLength(j_byte_array.obj());
  CHECK_EXCEPTION(jni) << "error during GetArrayLength";
  std::string str(len, '\0');
  jni->GetByteArrayRegion(j_byte_array.obj(), 0, len,
                          reinterpret_cast<jbyte*>(&str[0]));
  CHECK_EXCEPTION(jni) << "error during GetByteArrayRegion";
  return str;
}

// Given a list of jstrings, reinterprets it to a new vector of native strings.
std::vector<std::string> JavaToStdVectorStrings(JNIEnv* jni,
                                                const JavaRef<jobject>& list) {
  std::vector<std::string> converted_list;
  if (!list.is_null()) {
    for (const JavaRef<jobject>& str : Iterable(jni, list)) {
      converted_list.push_back(JavaToStdString(
          jni, JavaParamRef<jstring>(static_cast<jstring>(str.obj()))));
    }
  }
  return converted_list;
}

rtc::Optional<int32_t> JavaToNativeOptionalInt(
    JNIEnv* jni,
    const JavaRef<jobject>& integer) {
  if (IsNull(jni, integer))
    return rtc::nullopt;
  return JNI_Integer::Java_Integer_intValue(jni, integer);
}

rtc::Optional<bool> JavaToNativeOptionalBool(JNIEnv* jni,
                                             const JavaRef<jobject>& boolean) {
  if (IsNull(jni, boolean))
    return rtc::nullopt;
  return JNI_Boolean::Java_Boolean_booleanValue(jni, boolean);
}

int64_t JavaToNativeLong(JNIEnv* env, const JavaRef<jobject>& j_long) {
  return JNI_Long::Java_Long_longValue(env, j_long);
}

ScopedJavaLocalRef<jobject> NativeToJavaBoolean(JNIEnv* env, bool b) {
  return JNI_Boolean::Java_Boolean_ConstructorJLB_Z(env, b);
}

ScopedJavaLocalRef<jobject> NativeToJavaInteger(JNIEnv* jni, int32_t i) {
  return JNI_Integer::Java_Integer_ConstructorJLI_I(jni, i);
}

ScopedJavaLocalRef<jobject> NativeToJavaLong(JNIEnv* env, int64_t u) {
  return JNI_Long::Java_Long_ConstructorJLLO_J(env, u);
}

ScopedJavaLocalRef<jobject> NativeToJavaDouble(JNIEnv* env, double d) {
  return JNI_Double::Java_Double_ConstructorJLD_D(env, d);
}

ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* env, const char* str) {
  jstring j_str = env->NewStringUTF(str);
  CHECK_EXCEPTION(env) << "error during NewStringUTF";
  return ScopedJavaLocalRef<jstring>(env, j_str);
}

ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* jni,
                                               const std::string& str) {
  return NativeToJavaString(jni, str.c_str());
}

ScopedJavaLocalRef<jobject> NativeToJavaInteger(
    JNIEnv* jni,
    const rtc::Optional<int32_t>& optional_int) {
  return optional_int ? NativeToJavaInteger(jni, *optional_int) : nullptr;
}

std::string GetJavaEnumName(JNIEnv* jni, const JavaRef<jobject>& j_enum) {
  return JavaToStdString(jni, JNI_Enum::Java_Enum_name(jni, j_enum));
}

std::map<std::string, std::string> JavaToStdMapStrings(
    JNIEnv* jni,
    const JavaRef<jobject>& j_map) {
  const JavaRef<jobject>& j_entry_set = JNI_Map::Java_Map_entrySet(jni, j_map);
  std::map<std::string, std::string> result;
  for (const JavaRef<jobject>& j_entry : Iterable(jni, j_entry_set)) {
    result.insert(std::make_pair(
        JavaToStdString(jni, Java_JniHelper_getKey(jni, j_entry)),
        JavaToStdString(jni, Java_JniHelper_getValue(jni, j_entry))));
  }

  return result;
}

jobject NewGlobalRef(JNIEnv* jni, jobject o) {
  jobject ret = jni->NewGlobalRef(o);
  CHECK_EXCEPTION(jni) << "error during NewGlobalRef";
  RTC_CHECK(ret);
  return ret;
}

void DeleteGlobalRef(JNIEnv* jni, jobject o) {
  jni->DeleteGlobalRef(o);
  CHECK_EXCEPTION(jni) << "error during DeleteGlobalRef";
}

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
ScopedLocalRefFrame::ScopedLocalRefFrame(JNIEnv* jni) : jni_(jni) {
  RTC_CHECK(!jni_->PushLocalFrame(0)) << "Failed to PushLocalFrame";
}
ScopedLocalRefFrame::~ScopedLocalRefFrame() {
  jni_->PopLocalFrame(nullptr);
}

Iterable::Iterable(JNIEnv* jni, const JavaRef<jobject>& iterable)
    : jni_(jni), iterable_(jni, iterable) {}

Iterable::~Iterable() = default;

// Creates an iterator representing the end of any collection.
Iterable::Iterator::Iterator() = default;

// Creates an iterator pointing to the beginning of the specified collection.
Iterable::Iterator::Iterator(JNIEnv* jni, const JavaRef<jobject>& iterable)
    : jni_(jni) {
  iterator_ = JNI_Iterable::Java_Iterable_iterator(jni, iterable);
  RTC_CHECK(!iterator_.is_null());
  // Start at the first element in the collection.
  ++(*this);
}

// Move constructor - necessary to be able to return iterator types from
// functions.
Iterable::Iterator::Iterator(Iterator&& other)
    : jni_(std::move(other.jni_)),
      iterator_(std::move(other.iterator_)),
      value_(std::move(other.value_)),
      thread_checker_(std::move(other.thread_checker_)){};

Iterable::Iterator::~Iterator() = default;

// Advances the iterator one step.
Iterable::Iterator& Iterable::Iterator::operator++() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  if (AtEnd()) {
    // Can't move past the end.
    return *this;
  }
  bool has_next = JNI_Iterator::Java_Iterator_hasNext(jni_, iterator_);
  if (!has_next) {
    iterator_ = nullptr;
    value_ = nullptr;
    return *this;
  }

  value_ = JNI_Iterator::Java_Iterator_next(jni_, iterator_);
  return *this;
}

void Iterable::Iterator::Remove() {
  JNI_Iterator::Java_Iterator_remove(jni_, iterator_);
}

// Provides a way to compare the iterator with itself and with the end iterator.
// Note: all other comparison results are undefined, just like for C++ input
// iterators.
bool Iterable::Iterator::operator==(const Iterable::Iterator& other) {
  // Two different active iterators should never be compared.
  RTC_DCHECK(this == &other || AtEnd() || other.AtEnd());
  return AtEnd() == other.AtEnd();
}

ScopedJavaLocalRef<jobject>& Iterable::Iterator::operator*() {
  RTC_CHECK(!AtEnd());
  return value_;
}

bool Iterable::Iterator::AtEnd() const {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  return jni_ == nullptr || IsNull(jni_, iterator_);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaIntegerArray(
    JNIEnv* env,
    const std::vector<int32_t>& container) {
  ScopedJavaLocalRef<jobject> (*convert_function)(JNIEnv*, int32_t) =
      &NativeToJavaInteger;
  return NativeToJavaObjectArray(env, container, java_lang_Integer_clazz(env),
                                 convert_function);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& container) {
  return NativeToJavaObjectArray(env, container, java_lang_Boolean_clazz(env),
                                 &NativeToJavaBoolean);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaDoubleArray(
    JNIEnv* env,
    const std::vector<double>& container) {
  return NativeToJavaObjectArray(env, container, java_lang_Double_clazz(env),
                                 &NativeToJavaDouble);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaLongArray(
    JNIEnv* env,
    const std::vector<int64_t>& container) {
  return NativeToJavaObjectArray(env, container, java_lang_Long_clazz(env),
                                 &NativeToJavaLong);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaStringArray(
    JNIEnv* env,
    const std::vector<std::string>& container) {
  ScopedJavaLocalRef<jstring> (*convert)(JNIEnv*, const std::string&) =
      &NativeToJavaString;
  return NativeToJavaObjectArray(
      env, container,
      static_cast<jclass>(Java_JniHelper_getStringClass(env).obj()), convert);
}

JavaMapBuilder::JavaMapBuilder(JNIEnv* env)
    : env_(env),
      j_map_(JNI_LinkedHashMap::Java_LinkedHashMap_ConstructorJULIHM(env)) {}

JavaMapBuilder::~JavaMapBuilder() = default;

void JavaMapBuilder::put(const JavaRef<jobject>& key,
                         const JavaRef<jobject>& value) {
  JNI_Map::Java_Map_put(env_, j_map_, key, value);
}

JavaListBuilder::JavaListBuilder(JNIEnv* env)
    : env_(env), j_list_(JNI_ArrayList::Java_ArrayList_ConstructorJUALI(env)) {}

JavaListBuilder::~JavaListBuilder() = default;

void JavaListBuilder::add(const JavaRef<jobject>& element) {
  JNI_ArrayList::Java_ArrayList_addZ_JUE(env_, j_list_, element);
}

}  // namespace jni
}  // namespace webrtc
