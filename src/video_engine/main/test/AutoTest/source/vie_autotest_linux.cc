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
// vie_autotest_linux.cc
//

#include "gtest/gtest.h"
#include <string>

#include "vie_autotest_linux.h"

#include "vie_autotest_defines.h"
#include "vie_autotest_main.h"

#include "engine_configurations.h"
#include "critical_section_wrapper.h"
#include "thread_wrapper.h"

ViEAutoTestWindowManager::ViEAutoTestWindowManager() :
    _hdsp1(NULL),
    _hdsp2(NULL)
{
}

ViEAutoTestWindowManager::~ViEAutoTestWindowManager()
{
    TerminateWindows();
}

void* ViEAutoTestWindowManager::GetWindow1()
{
    return (void*) _hwnd1;
}

void* ViEAutoTestWindowManager::GetWindow2()
{
    return (void*) _hwnd2;
}

int ViEAutoTestWindowManager::TerminateWindows()
{
    if (_hdsp1)
    {
        ViEDestroyWindow(&_hwnd1, _hdsp1);
        _hdsp1 = NULL;
    }
    if (_hdsp2)
    {
        ViEDestroyWindow(&_hwnd2, _hdsp2);
        _hdsp2 = NULL;
    }
    return 0;
}

int ViEAutoTestWindowManager::CreateWindows(AutoTestRect window1Size,
                                            AutoTestRect window2Size,
                                            void* window1Title,
                                            void* window2Title)
{
    ViECreateWindow(&_hwnd1, &_hdsp1, window1Size.origin.x,
                    window1Size.origin.y, window1Size.size.width,
                    window1Size.size.height, (char*) window1Title);
    ViECreateWindow(&_hwnd2, &_hdsp2, window2Size.origin.x,
                    window2Size.origin.y, window2Size.size.width,
                    window2Size.size.height, (char*) window2Title);

    return 0;
}

int ViEAutoTestWindowManager::ViECreateWindow(Window *outWindow,
                                              Display **outDisplay, int xpos,
                                              int ypos, int width, int height,
                                              char* title)
{
    int screen;
    XEvent evnt;
    XSetWindowAttributes xswa; // window attribute struct
    XVisualInfo vinfo; // screen visual info struct
    unsigned long mask; // attribute mask

    // get connection handle to xserver
    Display* _display = XOpenDisplay(NULL);

    // get screen number
    screen = DefaultScreen(_display);

    // put desired visual info for the screen in vinfo 
    // TODO: more display settings should be allowed
    if (XMatchVisualInfo(_display, screen, 24, TrueColor, &vinfo) != 0)
    {
        //printf( "Screen visual info match!\n" );
    }
    // set window attributes 
    xswa.colormap = XCreateColormap(_display, DefaultRootWindow(_display),
                                    vinfo.visual, AllocNone);
    xswa.event_mask = StructureNotifyMask | ExposureMask;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;

    // value mask for attributes
    mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    Window _window = XCreateWindow(_display, DefaultRootWindow(_display), xpos,
                                   ypos, width, height, 0, vinfo.depth,
                                   InputOutput, vinfo.visual, mask, &xswa);

    // Set window name
    XStoreName(_display, _window, title);
    XSetIconName(_display, _window, title);

    // make x report events for mask 
    XSelectInput(_display, _window, StructureNotifyMask);

    // map the window to the display
    XMapWindow(_display, _window);

    // wait for map event
    do
    {
        XNextEvent(_display, &evnt);

    } while (evnt.type != MapNotify || evnt.xmap.event != _window);

    *outWindow = _window;
    *outDisplay = _display;
    return 0;
}

int ViEAutoTestWindowManager::ViEDestroyWindow(Window *window, Display *display)
{
    XUnmapWindow(display, *window);
    XDestroyWindow(display, *window);
    XSync(display, false);
    return 0;
}

bool ViEAutoTestWindowManager::SetTopmostWindow()
{
    return 0;
}

int main(int argc, char** argv)
{
    // This command-line flag is a transitory solution until we
    // managed to rewrite all tests to GUnit tests. This flag is
    // currently only supported in Linux.
    if (argc == 2 && std::string(argv[1]) == "--automated") {
      testing::InitGoogleTest(&argc, argv);
      return RUN_ALL_TESTS();
    }

    // Default: run in classic interactive mode.
    ViEAutoTestMain autoTest;
    autoTest.UseAnswerFile("answers.txt");
    return autoTest.BeginOSIndependentTesting();
}
