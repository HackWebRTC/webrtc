/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "Renderer.h"

#include <stdio.h>

#include <tchar.h>
#include <windows.h>

#include "thread_wrapper.h"
#include "tick_util.h"

#define SLEEP_10_SEC ::Sleep(10000)
#define GET_TIME_IN_MS timeGetTime

LRESULT CALLBACK WinProc( HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_DESTROY:
            break;
        case WM_COMMAND:
            break;
    }
    return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

namespace webrtc {

int WebRtcCreateWindow(void** os_specific_handle, int winNum, int width, int height)
{
    HWND* hwndMain = reinterpret_cast<HWND*> (os_specific_handle);  // HWND is a pointer type
    HINSTANCE hinst = GetModuleHandle(0);
    WNDCLASSEX wcx;
    wcx.hInstance = hinst;
    wcx.lpszClassName = _T(" test camera delay");
    wcx.lpfnWndProc = (WNDPROC)WinProc;
    wcx.style = CS_DBLCLKS;
    wcx.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hCursor = LoadCursor (NULL, IDC_ARROW);
    wcx.lpszMenuName = NULL;
    wcx.cbSize = sizeof (WNDCLASSEX);
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hbrBackground = GetSysColorBrush(COLOR_3DFACE);

    // Register our window class with the operating system.
    RegisterClassEx (&wcx);
    
    // Create the main window.
    *hwndMain = CreateWindowEx(0, // no extended styles
                               wcx.lpszClassName, // class name
                               _T("Test Camera Delay"), // window name
                               WS_OVERLAPPED |WS_THICKFRAME, // overlapped window
                               0, // horizontal position
                               0, // vertical position
                               width, // width
                               height, // height
                               (HWND) NULL, // no parent or owner window
                               (HMENU) NULL, // class menu used
                               hinst, // instance handle
                               NULL); // no window creation data

    if (!*hwndMain)
    {
        int error = GetLastError();
        return -1;
    }

    // Show the window using the flag specified by the program
    // that started the application, and send the application
    // a WM_PAINT message.

    ShowWindow(*hwndMain, SW_SHOWDEFAULT);
    UpdateWindow(*hwndMain);
    return 0;
}

void SetWindowPos(void* os_specific_handle, int x, int y, int width, int height, bool onTop)
{
    HWND hwndMain = (HWND)os_specific_handle;
    // Call the Windows API
    SetWindowPos(hwndMain, HWND_TOP, x, y, width, height, 0);
}

} // namespace webrtc 
