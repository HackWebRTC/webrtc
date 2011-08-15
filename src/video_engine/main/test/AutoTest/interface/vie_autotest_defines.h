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

#if defined(_WIN32)
#include <windows.h>
#elif defined (WEBRTC_ANDROID)
#include <android/log.h>
#include <string>
#elif defined(WEBRTC_LINUX)
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#elif defined(WEBRTC_MAC_INTEL)
#import <Foundation/Foundation.h>
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
#define DEFAULT_VIDEO_PORT				9000
#define DEFAULT_VIDEO_CODEC				"vp8"
#define DEFAULT_VIDEO_CODEC_WIDTH		352
#define DEFAULT_VIDEO_CODEC_HEIGHT		288
#define DEFAULT_AUDIO_PORT				8000
#define DEFAULT_AUDIO_CODEC				"isac"

enum
{
    KAutoTestSleepTimeMs = 5000
};

struct AutoTestSize
{
    unsigned int width;
    unsigned int height;
    AutoTestSize() :
        width(0),
        height(0)
    {}
    AutoTestSize(unsigned int iWidth, unsigned int iHeight) :
        width(iWidth),
        height(iHeight)
    {}
};

struct AutoTestOrigin
{
    unsigned int x;
    unsigned int y;
    AutoTestOrigin() :
        x(0),
        y(0)
    {}
    AutoTestOrigin(unsigned int iX, unsigned int iY) :
        x(iX),
        y(iY)
    {}
};

struct AutoTestRect
{
    AutoTestSize size;
    AutoTestOrigin origin;
    AutoTestRect() :
        size(),
        origin()
    {}

    AutoTestRect(unsigned int iX, unsigned int iY, unsigned int iWidth,
                 unsigned int iHeight) :
        size(iX, iY),
        origin(iWidth, iHeight)
    {}

    void Copy(AutoTestRect iRect)
    {
        origin.x = iRect.origin.x;
        origin.y = iRect.origin.y;
        size.width = iRect.size.width;
        size.height = iRect.size.height;
    }
};

// ============================================

class ViETest
{
protected:
    static FILE* _logFile;
    enum
    {
        KMaxLogSize = 512
    };
    static char* _logStr;
public:

    static int Init()
    {
#ifdef VIE_LOG_TO_FILE
        _logFile = fopen(VIE_LOG_FILE_NAME, "w+t");
#else
        _logFile = NULL;
#endif
        _logStr = new char[KMaxLogSize];
        memset(_logStr, 0, KMaxLogSize);
        return 0;
    }

    static int Terminate()
    {
        if (_logFile)
        {
            fclose(_logFile);
            _logFile = NULL;
        }
        if (_logStr)
        {
            delete[] _logStr;
            _logStr = NULL;
        }
        return 0;
    }

    static void Log(const char* fmt, ...)
    {
        va_list va;
        va_start(va, fmt);
        memset(_logStr, 0, KMaxLogSize);
        vsprintf(_logStr, fmt, va);
        va_end(va);

#ifdef VIE_LOG_TO_FILE
        if (_logFile)
        {
            fwrite(_logStr, 1, strlen(_logStr), _logFile);
            fwrite("\n", 1, 1, _logFile);
            fflush(_logFile);
        }
#endif
#ifdef VIE_LOG_TO_STDOUT
#if WEBRTC_ANDROID
        __android_log_write(ANDROID_LOG_DEBUG, "*WebRTCN*", _logStr);
#else
        printf("%s\n",_logStr);
#endif
#endif
    }

    static int TestError(bool expr)
    {
        if (!expr)
        {
#ifdef VIE_ASSERT_ERROR
            assert(expr);
#endif
            return 1;
        }
        return 0;
    }

    static int TestError(bool expr, const char* fmt, ...)
    {

        if (!expr)
        {
            va_list va;
            va_start(va, fmt);
            memset(_logStr, 0, KMaxLogSize);
            vsprintf(_logStr, fmt, va);
#ifdef WEBRTC_ANDROID
            __android_log_write(ANDROID_LOG_ERROR, "*WebRTCN*", _logStr);
#endif
            Log(_logStr);
            va_end(va);

#ifdef VIE_ASSERT_ERROR
            assert(false);
#endif
            return 1;
        }
        return 0;
    }
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

namespace
{
FILE* OpenTestFile(const char* fileName)
{
    char filePath[256];
    sprintf(filePath,"%s%s",VIE_TEST_FILES_ROOT,fileName);
    return fopen(filePath,"rb");
}
}
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_DEFINES_H_
