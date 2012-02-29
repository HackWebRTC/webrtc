vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "122775",

  # External resources like video and audio files used for testing purposes.
  # Downloaded on demand when needed.
  "webrtc_resources_revision": "8",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  "trunk/chromium_deps":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chromium_revision")),

  "trunk/build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  "trunk/testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "trunk/testing/gmock":
    From("trunk/chromium_deps", "src/testing/gmock"),

  "trunk/testing/gtest":
    From("trunk/chromium_deps", "src/testing/gtest"),

  "trunk/third_party/expat":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  # Used by tools/quality_tracking.
  "trunk/third_party/gaeunit":
    "http://code.google.com/p/gaeunit.git@e16d5bd4",

  "trunk/third_party/google-gflags/src":
    (Var("googlecode_url") % "google-gflags") + "/trunk/src@45",

  # Used by tools/quality_tracking/dashboard and tools/python_charts.
  "trunk/third_party/google-visualization-python":
    (Var("googlecode_url") % "google-visualization-python") + "/trunk@15",

  "trunk/third_party/libjpeg":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "trunk/third_party/libjpeg_turbo":
    From("trunk/chromium_deps", "src/third_party/libjpeg_turbo"),

  "trunk/third_party/libvpx/source/libvpx":
    "http://git.chromium.org/webm/libvpx.git@v1.0.0",

  "trunk/third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@190",

  "trunk/third_party/protobuf":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  # Used by tools/quality_tracking.
  "trunk/third_party/oauth2":
    "http://github.com/simplegeo/python-oauth2.git@a83f4a29",

  "trunk/third_party/yasm":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "trunk/third_party/yasm/source/patched-yasm":
    From("trunk/chromium_deps", "src/third_party/yasm/source/patched-yasm"),

  "trunk/tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chromium_revision"),

  "trunk/tools/gyp":
    From("trunk/chromium_deps", "src/tools/gyp"),

  "trunk/tools/python":
    Var("chromium_trunk") + "/src/tools/python@" + Var("chromium_revision"),

  "trunk/tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chromium_revision"),

  # Needed by build/common.gypi.
  "trunk/tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),
}

deps_os = {
  "win": {
    "trunk/third_party/cygwin":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",

    # Used by libjpeg-turbo.
    "trunk/third_party/yasm/binaries":
      From("trunk/chromium_deps", "src/third_party/yasm/binaries"),
  },
  "unix": {
    "trunk/third_party/gold":
      From("trunk/chromium_deps", "src/third_party/gold"),
  },
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

