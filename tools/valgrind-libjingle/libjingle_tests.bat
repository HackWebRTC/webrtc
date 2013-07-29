@echo off
:: libjingle
:: Copyright 2004--2010, Google Inc.
::
:: Redistribution and use in source and binary forms, with or without
:: modification, are permitted provided that the following conditions are met:
::
::  1. Redistributions of source code must retain the above copyright notice,
::     this list of conditions and the following disclaimer.
::  2. Redistributions in binary form must reproduce the above copyright notice,
::     this list of conditions and the following disclaimer in the documentation
::     and/or other materials provided with the distribution.
::  3. The name of the author may not be used to endorse or promote products
::     derived from this software without specific prior written permission.
::
:: THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
:: WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
:: MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
:: EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
:: SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
:: PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
:: OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
:: WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
:: OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
:: ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

:: This script is a copy of chrome_tests.bat with the following changes:
:: - Invokes libjingle_tests.py instead of chrome_tests.py
:: - Chromium's Valgrind scripts directory is added to the PYTHONPATH to make
::   it possible to execute the Python scripts properly.

:: TODO(timurrrr): batch files 'export' all the variables to the parent shell
set THISDIR=%~dp0
set TOOL_NAME="unknown"

:: Get the tool name and put it into TOOL_NAME {{{1
:: NB: SHIFT command doesn't modify %*
:PARSE_ARGS_LOOP
  if %1 == () GOTO:TOOLNAME_NOT_FOUND
  if %1 == --tool GOTO:TOOLNAME_FOUND
  SHIFT
  goto :PARSE_ARGS_LOOP

:TOOLNAME_NOT_FOUND
echo "Please specify a tool (tsan or drmemory) by using --tool flag"
exit /B 1

:TOOLNAME_FOUND
SHIFT
set TOOL_NAME=%1
:: }}}
if "%TOOL_NAME%" == "drmemory"          GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_light"    GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_full"     GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_pattern"  GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "tsan"     GOTO :SETUP_TSAN
echo "Unknown tool: `%TOOL_NAME%`! Only tsan and drmemory are supported."
exit /B 1

:SETUP_DRMEMORY
if NOT "%DRMEMORY_COMMAND%"=="" GOTO :RUN_TESTS
:: Set up DRMEMORY_COMMAND to invoke Dr. Memory {{{1
set DRMEMORY_PATH=%THISDIR%..\..\third_party\drmemory
set DRMEMORY_SFX=%DRMEMORY_PATH%\drmemory-windows-sfx.exe
if EXIST %DRMEMORY_SFX% GOTO DRMEMORY_BINARY_OK
echo "Can't find Dr. Memory executables."
echo "See http://www.chromium.org/developers/how-tos/using-valgrind/dr-memory"
echo "for the instructions on how to get them."
exit /B 1

:DRMEMORY_BINARY_OK
%DRMEMORY_SFX% -o%DRMEMORY_PATH%\unpacked -y
set DRMEMORY_COMMAND=%DRMEMORY_PATH%\unpacked\bin\drmemory.exe
:: }}}
goto :RUN_TESTS

:SETUP_TSAN
:: Set up PIN_COMMAND to invoke TSan {{{1
set TSAN_PATH=%THISDIR%..\..\third_party\tsan
set TSAN_SFX=%TSAN_PATH%\tsan-x86-windows-sfx.exe
if EXIST %TSAN_SFX% GOTO TSAN_BINARY_OK
echo "Can't find ThreadSanitizer executables."
echo "See http://www.chromium.org/developers/how-tos/using-valgrind/threadsanitizer/threadsanitizer-on-windows"
echo "for the instructions on how to get them."
exit /B 1

:TSAN_BINARY_OK
%TSAN_SFX% -o%TSAN_PATH%\unpacked -y
set PIN_COMMAND=%TSAN_PATH%\unpacked\tsan-x86-windows\tsan.bat
:: }}}
goto :RUN_TESTS

:RUN_TESTS
set PYTHONPATH=%THISDIR%..\python\google;%THISDIR%..\valgrind
set RUNNING_ON_VALGRIND=yes
python %THISDIR%libjingle_tests.py %*
