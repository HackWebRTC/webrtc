/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "Logger.h"
#include "string.h"
#include "file_wrapper.h"

namespace webrtc
{

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif
Logger::Logger() :
    _logFile(*FileWrapper::Create())
{
}

Logger::~Logger(void)
{
    if (_logFile.Open())
        _logFile.CloseFile();
}
void Logger::Print(char* msg)
{
    printf("%s\n",msg);
    if (_logFile.Open())
    {
        _logFile.WriteText(msg);
    }
}
#define BUFSIZE 256

void Logger::SetFileName(const char* fileName)
{
    _logFile.CloseFile();
    if (!fileName)
        return;
    _logFile.OpenFile(fileName, false, false, true);
    char osVersion[BUFSIZE];
    memset(osVersion, 0, sizeof(osVersion));

    GetOSDisplayString(osVersion);
    _logFile.WriteText(osVersion);
    _logFile.WriteText("\n\n");
}

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

bool Logger::GetOSDisplayString( void* psz)
{
    OSVERSIONINFOEX osvi;
    SYSTEM_INFO si;
    PGNSI pGNSI;
    PGPI pGPI;
    BOOL bOsVersionInfoEx;
    DWORD dwType;
    STRSAFE_LPWSTR pszOS = (STRSAFE_LPWSTR) psz;
    size_t bufferSize = BUFSIZE/sizeof(TCHAR);

    ZeroMemory(&si, sizeof(SYSTEM_INFO));
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
    return 1;

    // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

    pGNSI = (PGNSI) GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")),
        "GetNativeSystemInfo");
    if(NULL != pGNSI)
    pGNSI(&si);
    else GetSystemInfo(&si);

    if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId &&
        osvi.dwMajorVersion > 4 )
    {
        StringCchCopy(pszOS, bufferSize, TEXT("Microsoft "));

        // Test for the specific product.

        if ( osvi.dwMajorVersion == 6 )
        {
            if( osvi.dwMinorVersion == 0 )
            {
                if( osvi.wProductType == VER_NT_WORKSTATION )
                StringCchCat(pszOS, bufferSize, TEXT("Windows Vista "));
                else StringCchCat(pszOS, bufferSize, TEXT("Windows Server 2008 " ));
            }

            if ( osvi.dwMinorVersion == 1 )
            {
                if( osvi.wProductType == VER_NT_WORKSTATION )
                StringCchCat(pszOS, bufferSize, TEXT("Windows 7 "));
                else StringCchCat(pszOS, bufferSize, TEXT("Windows Server 2008 R2 " ));
            }

            pGPI = (PGPI) GetProcAddress(
                GetModuleHandle(TEXT("kernel32.dll")),
                "GetProductInfo");

            pGPI( osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

            switch( dwType )
            {
                case PRODUCT_ULTIMATE:
                StringCchCat(pszOS, bufferSize, TEXT("Ultimate Edition" ));
                break;
                //            case PRODUCT_PROFESSIONAL:
                //             StringCchCat(pszOS, bufferSize, TEXT("Professional" ));
                break;
                case PRODUCT_HOME_PREMIUM:
                StringCchCat(pszOS, bufferSize, TEXT("Home Premium Edition" ));
                break;
                case PRODUCT_HOME_BASIC:
                StringCchCat(pszOS, bufferSize, TEXT("Home Basic Edition" ));
                break;
                case PRODUCT_ENTERPRISE:
                StringCchCat(pszOS, bufferSize, TEXT("Enterprise Edition" ));
                break;
                case PRODUCT_BUSINESS:
                StringCchCat(pszOS, bufferSize, TEXT("Business Edition" ));
                break;
                case PRODUCT_STARTER:
                StringCchCat(pszOS, bufferSize, TEXT("Starter Edition" ));
                break;
                case PRODUCT_CLUSTER_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Cluster Server Edition" ));
                break;
                case PRODUCT_DATACENTER_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Datacenter Edition" ));
                break;
                case PRODUCT_DATACENTER_SERVER_CORE:
                StringCchCat(pszOS, bufferSize, TEXT("Datacenter Edition (core installation)" ));
                break;
                case PRODUCT_ENTERPRISE_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Enterprise Edition" ));
                break;
                case PRODUCT_ENTERPRISE_SERVER_CORE:
                StringCchCat(pszOS, bufferSize, TEXT("Enterprise Edition (core installation)" ));
                break;
                case PRODUCT_ENTERPRISE_SERVER_IA64:
                StringCchCat(pszOS, bufferSize, TEXT("Enterprise Edition for Itanium-based Systems" ));
                break;
                case PRODUCT_SMALLBUSINESS_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Small Business Server" ));
                break;
                case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
                StringCchCat(pszOS, bufferSize, TEXT("Small Business Server Premium Edition" ));
                break;
                case PRODUCT_STANDARD_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Standard Edition" ));
                break;
                case PRODUCT_STANDARD_SERVER_CORE:
                StringCchCat(pszOS, bufferSize, TEXT("Standard Edition (core installation)" ));
                break;
                case PRODUCT_WEB_SERVER:
                StringCchCat(pszOS, bufferSize, TEXT("Web Server Edition" ));
                break;
            }
        }

        if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
        {
            if( GetSystemMetrics(SM_SERVERR2) )
            StringCchCat(pszOS, bufferSize, TEXT( "Windows Server 2003 R2, "));
            else if ( osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER )
            StringCchCat(pszOS, bufferSize, TEXT( "Windows Storage Server 2003"));
            //else if ( osvi.wSuiteMask & VER_SUITE_WH_SERVER )

            // StringCchCat(pszOS, bufferSize, TEXT( "Windows Home Server"));

            else if( osvi.wProductType == VER_NT_WORKSTATION &&
                si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
            {
                StringCchCat(pszOS, bufferSize, TEXT( "Windows XP Professional x64 Edition"));
            }
            else StringCchCat(pszOS, bufferSize, TEXT("Windows Server 2003, "));

            // Test for the server type.
            if ( osvi.wProductType != VER_NT_WORKSTATION )
            {
                if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
                {
                    if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                    StringCchCat(pszOS, bufferSize, TEXT( "Datacenter Edition for Itanium-based Systems" ));
                    else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                    StringCchCat(pszOS, bufferSize, TEXT( "Enterprise Edition for Itanium-based Systems" ));
                }

                else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
                {
                    if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                    StringCchCat(pszOS, bufferSize, TEXT( "Datacenter x64 Edition" ));
                    else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                    StringCchCat(pszOS, bufferSize, TEXT( "Enterprise x64 Edition" ));
                    else StringCchCat(pszOS, bufferSize, TEXT( "Standard x64 Edition" ));
                }

                else
                {
                    if ( osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
                    StringCchCat(pszOS, bufferSize, TEXT( "Compute Cluster Edition" ));
                    else if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                    StringCchCat(pszOS, bufferSize, TEXT( "Datacenter Edition" ));
                    else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                    StringCchCat(pszOS, bufferSize, TEXT( "Enterprise Edition" ));
                    else if ( osvi.wSuiteMask & VER_SUITE_BLADE )
                    StringCchCat(pszOS, bufferSize, TEXT( "Web Edition" ));
                    else StringCchCat(pszOS, bufferSize, TEXT( "Standard Edition" ));
                }
            }
        }

        if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
        {
            StringCchCat(pszOS, bufferSize, TEXT("Windows XP "));
            if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
            StringCchCat(pszOS, bufferSize, TEXT( "Home Edition" ));
            else StringCchCat(pszOS, bufferSize, TEXT( "Professional" ));
        }

        if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
        {
            StringCchCat(pszOS, bufferSize, TEXT("Windows 2000 "));

            if ( osvi.wProductType == VER_NT_WORKSTATION )
            {
                StringCchCat(pszOS, bufferSize, TEXT( "Professional" ));
            }
            else
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                StringCchCat(pszOS, bufferSize, TEXT( "Datacenter Server" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                StringCchCat(pszOS, bufferSize, TEXT( "Advanced Server" ));
                else StringCchCat(pszOS, bufferSize, TEXT( "Server" ));
            }
        }

        // Include service pack (if any) and build number.

        if( _tcslen(osvi.szCSDVersion) > 0 )
        {
            StringCchCat(pszOS, bufferSize, TEXT(" ") );
            StringCchCat(pszOS, bufferSize, osvi.szCSDVersion);
        }

        TCHAR buf[80];

        StringCchPrintf( buf, 80, TEXT(" (build %d)"), osvi.dwBuildNumber);
        StringCchCat(pszOS, bufferSize, buf);

        if ( osvi.dwMajorVersion >= 6 )
        {
            if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            StringCchCat(pszOS, bufferSize, TEXT( ", 64-bit" ));
            else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
            StringCchCat(pszOS, bufferSize, TEXT(", 32-bit"));
        }
        StringCchPrintf( buf, 80, TEXT(" (number of processors %d)"), si.dwNumberOfProcessors);
        StringCchCat(pszOS, bufferSize, buf);

        return TRUE;
    }

    else
    {
        printf( "This sample does not support this version of Windows.\n");
        return FALSE;
    }
}

#elif defined(WEBRTC_MAC_INTEL)
bool Logger::GetOSDisplayString(void* psz)
{}

#elif defined(WEBRTC_LINUX)

bool Logger::GetOSDisplayString(void* /*psz*/)
{   return true;}

#endif
} // namespace webrtc
