vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "114939",
  "libjingle_revision": "101",

  # External resources like video and audio files used for testing purposes.
  # Downloaded on demand when needed.
  "webrtc_resources_revision": "6",
}

# NOTE: Prefer revision numbers to tags for svn deps.
deps = {
  "trunk/build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  "trunk/testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "trunk/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@573",

  "trunk/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@386",

  "trunk/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@1107",

  # Needed by build/common.gypi.
  "trunk/tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),

  "trunk/tools/clang/scripts":
    Var("chromium_trunk") + "/src/tools/clang/scripts@" + Var("chromium_revision"),

  "trunk/tools/python":
    Var("chromium_trunk") + "/src/tools/python@" + Var("chromium_revision"),

  "trunk/tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chromium_revision"),

  "trunk/third_party/protobuf/":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  "trunk/third_party/libvpx/source/libvpx":
    "http://git.chromium.org/webm/libvpx.git@bdd35c13",

  "trunk/third_party/libjpeg_turbo/":
    Var("chromium_trunk") + "/deps/third_party/libjpeg_turbo@95800",

  "trunk/third_party/libjpeg/":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "trunk/third_party/libsrtp/":
    Var("chromium_trunk") + "/deps/third_party/libsrtp@115467",

  "trunk/third_party/yasm/":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "trunk/third_party/expat/":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  "trunk/third_party/libjingle/":
    Var("chromium_trunk") + "/src/third_party/libjingle@" + Var("chromium_revision"),

  "trunk/third_party/google-gflags/src":
    (Var("googlecode_url") % "google-gflags") + "/trunk/src@45",

  "trunk/third_party/libjingle/source":
    (Var("googlecode_url") % "libjingle") + "/trunk@" + Var("libjingle_revision"),

  "trunk/third_party/yasm/source/patched-yasm":
    Var("chromium_trunk") + "/deps/third_party/yasm/patched-yasm@73761",
  # Used by libjpeg-turbo
  "trunk/third_party/yasm/binaries":
    Var("chromium_trunk") + "/deps/third_party/yasm/binaries@74228",

  "trunk/third_party/jsoncpp/":
    "http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp@246",

  "trunk/third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@121",
    
  "trunk/third_party/google-visualization-python":
    (Var("googlecode_url") % "google-visualization-python") + "/trunk@15",
}

deps_os = {
  "win": {
    "trunk/third_party/cygwin/":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",
  }
}

hooks = [
  {
    # Create a supplement.gypi file under trunk/.  This file will be picked up
    # by gyp and we use it to set Chromium related variables (inside_chromium_build)
    # to 0 and enable the standalone build.
    "pattern": ".",
    "action": ["python", "trunk/tools/create_supplement_gypi.py", "trunk/src/supplement.gypi"],
  },
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang.
    "pattern": ".",
    "action": ["python", "trunk/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # Download test resources, i.e. video and audio files. If the latest
    # version is already downloaded, this takes zero seconds to run.
    # If a newer version or no current download exists, it will download
    # the resources and extract them.
    "pattern": ".",
    "action": ["python", "trunk/tools/resources/update.py"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_chromium", "--depth=trunk", "trunk/webrtc.gyp"],
  },
]

