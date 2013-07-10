// Copyright 2007 Google Inc. All Rights Reserved.

//         juberti@google.com (Justin Uberti)
//
// A reuseable entry point for gunit tests.

#ifdef WIN32
#include <crtdbg.h>
#endif

#include "talk/base/flags.h"
#include "talk/base/fileutils.h"
#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"

DEFINE_bool(help, false, "prints this message");
DEFINE_string(log, "", "logging options to use");
#ifdef WIN32
DEFINE_int(crt_break_alloc, -1, "memory allocation to break on");
DEFINE_bool(default_error_handlers, false,
            "leave the default exception/dbg handler functions in place");

void TestInvalidParameterHandler(const wchar_t* expression,
                                 const wchar_t* function,
                                 const wchar_t* file,
                                 unsigned int line,
                                 uintptr_t pReserved) {
  LOG(LS_ERROR) << "InvalidParameter Handler called.  Exiting.";
  LOG(LS_ERROR) << expression << std::endl << function << std::endl << file
                << std::endl << line;
  exit(1);
}
void TestPureCallHandler() {
  LOG(LS_ERROR) << "Purecall Handler called.  Exiting.";
  exit(1);
}
int TestCrtReportHandler(int report_type, char* msg, int* retval) {
    LOG(LS_ERROR) << "CrtReport Handler called...";
    LOG(LS_ERROR) << msg;
  if (report_type == _CRT_ASSERT) {
    exit(1);
  } else {
    *retval = 0;
    return TRUE;
  }
}
#endif  // WIN32

talk_base::Pathname GetTalkDirectory() {
  // Locate talk directory.
  talk_base::Pathname path = talk_base::Filesystem::GetCurrentDirectory();
  std::string talk_folder_name("talk");
  talk_folder_name += path.folder_delimiter();
  while (path.folder_name() != talk_folder_name && !path.empty()) {
    path.SetFolder(path.parent_folder());
  }

  // If not running inside "talk" folder, then assume running in its parent
  // folder.
  if (path.empty()) {
    path = talk_base::Filesystem::GetCurrentDirectory();
    path.AppendFolder("talk");
    // Make sure the folder exist.
    if (!talk_base::Filesystem::IsFolder(path)) {
      path.clear();
    }
  }
  return path;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  FlagList::SetFlagsFromCommandLine(&argc, argv, false);
  if (FLAG_help) {
    FlagList::Print(NULL, false);
    return 0;
  }

#ifdef WIN32
  if (!FLAG_default_error_handlers) {
    // Make sure any errors don't throw dialogs hanging the test run.
    _set_invalid_parameter_handler(TestInvalidParameterHandler);
    _set_purecall_handler(TestPureCallHandler);
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, TestCrtReportHandler);
  }

#ifdef _DEBUG  // Turn on memory leak checking on Windows.
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF |_CRTDBG_LEAK_CHECK_DF);
  if (FLAG_crt_break_alloc >= 0) {
    _crtBreakAlloc = FLAG_crt_break_alloc;
  }
#endif  // _DEBUG
#endif  // WIN32

  talk_base::Filesystem::SetOrganizationName("google");
  talk_base::Filesystem::SetApplicationName("unittest");

  // By default, log timestamps. Allow overrides by used of a --log flag.
  talk_base::LogMessage::LogTimestamps();
  if (*FLAG_log != '\0') {
    talk_base::LogMessage::ConfigureLogging(FLAG_log, "unittest.log");
  }

  int res = RUN_ALL_TESTS();

  // clean up logging so we don't appear to leak memory.
  talk_base::LogMessage::ConfigureLogging("", "");

#ifdef WIN32
  // Unhook crt function so that we don't ever log after statics have been
  // uninitialized.
  if (!FLAG_default_error_handlers)
    _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, TestCrtReportHandler);
#endif

  return res;
}
