/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_defines.h
//


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_

#include <cassert>
#include <stdarg.h>
#include <stdio.h>

#include "engine_configurations.h"
#include "gtest/gtest.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined (WEBRTC_ANDROID)
#include <android/log.h>
#include <string>
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#endif

// Choose how to log
//#define VIE_LOG_TO_FILE
#define VIE_LOG_TO_STDOUT

// Choose one way to test error
#define VIE_ASSERT_ERROR

#define VIE_LOG_FILE_NAME "ViEAutotestLog.txt"

#undef RGB
#define RGB(r,g,b) r|g<<8|b<<16

// Default values for custom call
#define DEFAULT_SEND_IP					"127.0.0.1"
#define DEFAULT_VIDEO_PORT                              11111
#define DEFAULT_VIDEO_CODEC				"vp8"
#define DEFAULT_VIDEO_CODEC_WIDTH                       352
#define DEFAULT_VIDEO_CODEC_HEIGHT                      288
#define DEFAULT_VIDEO_CODEC_BITRATE                     100
#define DEFAULT_AUDIO_PORT                              11113
#define DEFAULT_AUDIO_CODEC				"isac"
#define DEFAULT_INCOMING_FILE_NAME                      "IncomingFile.avi"
#define DEFAULT_OUTGOING_FILE_NAME                      "OutgoingFile.avi"   

enum {
  KAutoTestSleepTimeMs = 5000
};

struct AutoTestSize {
  unsigned int width;
  unsigned int height;
  AutoTestSize() :
    width(0), height(0) {
  }
  AutoTestSize(unsigned int iWidth, unsigned int iHeight) :
    width(iWidth), height(iHeight) {
  }
};

struct AutoTestOrigin {
  unsigned int x;
  unsigned int y;
  AutoTestOrigin() :
    x(0), y(0) {
  }
  AutoTestOrigin(unsigned int iX, unsigned int iY) :
    x(iX), y(iY) {
  }
};

struct AutoTestRect {
  AutoTestSize size;
  AutoTestOrigin origin;
  AutoTestRect() :
    size(), origin() {
  }

  AutoTestRect(unsigned int iX, unsigned int iY, unsigned int iWidth, unsigned int iHeight) :
    size(iX, iY), origin(iWidth, iHeight) {
  }

  void Copy(AutoTestRect iRect) {
    origin.x = iRect.origin.x;
    origin.y = iRect.origin.y;
    size.width = iRect.size.width;
    size.height = iRect.size.height;
  }
};

// ============================================

class ViETest {
public:
  enum TestErrorMode {
    kUseGTestExpectsForTestErrors, kUseAssertsForTestErrors
  };

  // The test error mode tells how we should assert when an error
  // occurs, provided that VIE_ASSERT_ERROR is defined.
  static int Init(TestErrorMode test_error_mode) {
#ifdef VIE_LOG_TO_FILE
    log_file_ = fopen(VIE_LOG_FILE_NAME, "w+t");
#else
    log_file_ = NULL;
#endif
    log_str_ = new char[kMaxLogSize];
    memset(log_str_, 0, kMaxLogSize);

    test_error_mode_ = test_error_mode;
    return 0;
  }

  static int Terminate() {
    if (log_file_) {
      fclose(log_file_);
      log_file_ = NULL;
    }
    if (log_str_) {
      delete[] log_str_;
      log_str_ = NULL;
    }
    return 0;
  }

  static void Log(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    memset(log_str_, 0, kMaxLogSize);
    vsprintf(log_str_, fmt, va);
    va_end(va);

    WriteToSuitableOutput(log_str_);
  }

  // Writes to a suitable output, depending on platform and log mode.
  static void WriteToSuitableOutput(const char* message) {
#ifdef VIE_LOG_TO_FILE
    if (log_file_)
    {
      fwrite(log_str_, 1, strlen(log_str_), log_file_);
      fwrite("\n", 1, 1, log_file_);
      fflush(log_file_);
    }
#endif
#ifdef VIE_LOG_TO_STDOUT
#if WEBRTC_ANDROID
    __android_log_write(ANDROID_LOG_DEBUG, "*WebRTCN*", log_str_);
#else
    printf("%s\n", log_str_);
#endif
#endif
  }

  static int TestError(bool expr) {
    if (!expr) {
      AssertError("");
      return 1;
    }
    return 0;
  }

  static int TestError(bool expr, const char* fmt, ...) {
    if (!expr) {
      va_list va;
      va_start(va, fmt);
      memset(log_str_, 0, kMaxLogSize);
      vsprintf(log_str_, fmt, va);
#ifdef WEBRTC_ANDROID
      __android_log_write(ANDROID_LOG_ERROR, "*WebRTCN*", log_str_);
#endif
      WriteToSuitableOutput(log_str_);
      va_end(va);

      AssertError(log_str_);
      return 1;
    }
    return 0;
  }

private:
  static void AssertError(const char* message) {
#ifdef VIE_ASSERT_ERROR
    if (test_error_mode_ == kUseAssertsForTestErrors) {
      assert(false);
    } else if (test_error_mode_ == kUseGTestExpectsForTestErrors) {
      // Note that the failure gets added here, but information about where
      // the real error occurred is usually in the message.
      ADD_FAILURE() << message ;
    } else {
      assert(false && "Internal test framework logical error: unknown mode");
    }
#endif
  }

  static FILE* log_file_;
  enum {
    kMaxLogSize = 512
  };
  static char* log_str_;

  static TestErrorMode test_error_mode_;
};

// milliseconds
#if defined(_WIN32)
#define AutoTestSleep ::Sleep
#elif defined(WEBRTC_MAC_INTEL)
#define AutoTestSleep(x) usleep(x * 1000)
#elif defined(WEBRTC_LINUX)
namespace {
  void Sleep(unsigned long x) {
    timespec t;
    t.tv_sec = x/1000;
    t.tv_nsec = (x-(x/1000)*1000)*1000000;
    nanosleep(&t,NULL);
  }
}
#define AutoTestSleep ::Sleep
#endif

#ifdef WEBRTC_ANDROID
namespace {
  void Sleep(unsigned long x) {
    timespec t;
    t.tv_sec = x/1000;
    t.tv_nsec = (x-(x/1000)*1000)*1000000;
    nanosleep(&t,NULL);
  }
}

#define AutoTestSleep ::Sleep
#define VIE_TEST_FILES_ROOT "/sdcard/vie_auto_test/"
#else
#define VIE_TEST_FILES_ROOT "/tmp/"
#endif

namespace {
FILE* OpenTestFile(const char* fileName) {
  char filePath[256];
  sprintf(filePath, "%s%s", VIE_TEST_FILES_ROOT, fileName);
  return fopen(filePath, "rb");
}
}
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_
