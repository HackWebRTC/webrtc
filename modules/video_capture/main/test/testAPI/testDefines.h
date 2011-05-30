/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  testDefines.h
 */



#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTDEFINES_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTDEFINES_H_


#if defined (ANDROID)
#include <android/log.h>
#include <unistd.h>
#endif




#if defined(_WIN32)
	#define SLEEP(x) Sleep(x)
	#define SPRINTF(x, y, z, ...) sprintf_s(x, y, z, __VA_ARGS__)
	#define LOG(...) {						\
			char msg[512];					\
			sprintf_s(msg,512,__VA_ARGS__);	\
			_logger.Print(msg);				\
			}	
#elif defined (ANDROID)
    #define LOG(...) {             \
        char msg[512];                  \
        sprintf(msg,__VA_ARGS__); \
        __android_log_print(ANDROID_LOG_DEBUG, "VideoCaptureModule -testAPI", __VA_ARGS__);\
        _logger.Print(msg);             \
        }
    #define SLEEP(x) usleep(x*1000)
    #define SPRINTF(x, y, z, ...) sprintf(x, z, __VA_ARGS__)
#else
    #include <unistd.h>
	#define SLEEP(x) usleep(x * 1000)
	#define SPRINTF(x, y, z, ...) sprintf(x, z, __VA_ARGS__)
	#define LOG(...) {					\
			char msg[512];				\
			sprintf(msg, __VA_ARGS__);	\
			printf("%s\n", msg);	\
			}

#endif

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTDEFINES_H_
