/*
 * libjingle
 * Copyright 2015 Google Inc.
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
 *
 */
#include "talk/app/webrtc/java/jni/jni_helpers.h"

#include <asm/unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace webrtc_jni {

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
  void* env = NULL;
  jint status = g_jvm->GetEnv(&env, JNI_VERSION_1_6);
  RTC_CHECK(((env != NULL) && (status == JNI_OK)) ||
            ((env == NULL) && (status == JNI_EDETACHED)))
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
  args.group = NULL;
  // Deal with difference in signatures between Oracle's jni.h and Android's.
#ifdef _JAVASOFT_JNI_H_  // Oracle's jni.h violates the JNI spec!
  void* env = NULL;
#else
  JNIEnv* env = NULL;
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

// JNIEnv-helper methods that RTC_CHECK success: no Java exception thrown and
// found object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_EXCEPTION(jni) << "error during GetMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jmethodID GetStaticMethodID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jmethodID m = jni->GetStaticMethodID(c, name, signature);
  CHECK_EXCEPTION(jni) << "error during GetStaticMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jfieldID GetFieldID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jfieldID f = jni->GetFieldID(c, name, signature);
  CHECK_EXCEPTION(jni) << "error during GetFieldID";
  RTC_CHECK(f) << name << ", " << signature;
  return f;
}

jclass GetObjectClass(JNIEnv* jni, jobject object) {
  jclass c = jni->GetObjectClass(object);
  CHECK_EXCEPTION(jni) << "error during GetObjectClass";
  RTC_CHECK(c) << "GetObjectClass returned NULL";
  return c;
}

jobject GetObjectField(JNIEnv* jni, jobject object, jfieldID id) {
  jobject o = jni->GetObjectField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetObjectField";
  RTC_CHECK(o) << "GetObjectField returned NULL";
  return o;
}

jstring GetStringField(JNIEnv* jni, jobject object, jfieldID id) {
  return static_cast<jstring>(GetObjectField(jni, object, id));
}

jlong GetLongField(JNIEnv* jni, jobject object, jfieldID id) {
  jlong l = jni->GetLongField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetLongField";
  return l;
}

jint GetIntField(JNIEnv* jni, jobject object, jfieldID id) {
  jint i = jni->GetIntField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetIntField";
  return i;
}

bool GetBooleanField(JNIEnv* jni, jobject object, jfieldID id) {
  jboolean b = jni->GetBooleanField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetBooleanField";
  return b;
}

// Java references to "null" can only be distinguished as such in C++ by
// creating a local reference, so this helper wraps that logic.
bool IsNull(JNIEnv* jni, jobject obj) {
  ScopedLocalRefFrame local_ref_frame(jni);
  return jni->NewLocalRef(obj) == NULL;
}

// Given a UTF-8 encoded |native| string return a new (UTF-16) jstring.
jstring JavaStringFromStdString(JNIEnv* jni, const std::string& native) {
  jstring jstr = jni->NewStringUTF(native.c_str());
  CHECK_EXCEPTION(jni) << "error during NewStringUTF";
  return jstr;
}

// Given a (UTF-16) jstring return a new UTF-8 native string.
std::string JavaToStdString(JNIEnv* jni, const jstring& j_string) {
  const char* chars = jni->GetStringUTFChars(j_string, NULL);
  CHECK_EXCEPTION(jni) << "Error during GetStringUTFChars";
  std::string str(chars, jni->GetStringUTFLength(j_string));
  CHECK_EXCEPTION(jni) << "Error during GetStringUTFLength";
  jni->ReleaseStringUTFChars(j_string, chars);
  CHECK_EXCEPTION(jni) << "Error during ReleaseStringUTFChars";
  return str;
}

// Return the (singleton) Java Enum object corresponding to |index|;
jobject JavaEnumFromIndex(JNIEnv* jni, jclass state_class,
                          const std::string& state_class_name, int index) {
  jmethodID state_values_id = GetStaticMethodID(
      jni, state_class, "values", ("()[L" + state_class_name  + ";").c_str());
  jobjectArray state_values = static_cast<jobjectArray>(
      jni->CallStaticObjectMethod(state_class, state_values_id));
  CHECK_EXCEPTION(jni) << "error during CallStaticObjectMethod";
  jobject ret = jni->GetObjectArrayElement(state_values, index);
  CHECK_EXCEPTION(jni) << "error during GetObjectArrayElement";
  return ret;
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
  jni_->PopLocalFrame(NULL);
}

}  // namespace webrtc_jni
