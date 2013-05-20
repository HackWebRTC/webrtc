use_relative_paths = True

vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  "root_dir": "trunk",
  "extra_gyp_flag": "-Dextra_gyp_flag=0",

  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "199267",

  # External resources like video and audio files used for testing purposes.
  # Downloaded on demand when needed.
  "webrtc_resources_revision": "16",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  "../chromium_deps":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chromium_revision")),

  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  # Needed by common.gypi.
  "google_apis/build":
    Var("chromium_trunk") + "/src/google_apis/build@" + Var("chromium_revision"),

  "testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "testing/gmock":
    From("chromium_deps", "src/testing/gmock"),

  "testing/gtest":
    From("chromium_deps", "src/testing/gtest"),

  "third_party/expat":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  "third_party/google-gflags/src":
    (Var("googlecode_url") % "google-gflags") + "/trunk/src@45",

  "third_party/libjpeg":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "third_party/libjpeg_turbo":
    From("chromium_deps", "src/third_party/libjpeg_turbo"),

  "third_party/libvpx":
    Var("chromium_trunk") + "/deps/third_party/libvpx@192165",

  "third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@682",

  "third_party/opus":
    Var("chromium_trunk") + "/src/third_party/opus@185405",

  "third_party/opus/src":
    Var("chromium_trunk") + "/deps/third_party/opus@185324",

  "third_party/protobuf":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  "third_party/yasm":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "third_party/yasm/source/patched-yasm":
    From("chromium_deps", "src/third_party/yasm/source/patched-yasm"),

  "tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chromium_revision"),

  "tools/gyp":
    From("chromium_deps", "src/tools/gyp"),

  "tools/protoc_wrapper":
    Var("chromium_trunk") + "/src/tools/protoc_wrapper@" + Var("chromium_revision"),

  "tools/python":
    Var("chromium_trunk") + "/src/tools/python@" + Var("chromium_revision"),

  "tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chromium_revision"),

  # Needed by build/common.gypi.
  "tools/win/supalink":
    Var("chromium_trunk") + "/src/tools/win/supalink@" + Var("chromium_revision"),
}

deps_os = {
  "win": {
    # Use our own, stripped down, version of Cygwin (required by GYP).
    "third_party/cygwin":
      (Var("googlecode_url") % "webrtc") + "/deps/third_party/cygwin@2672",

    "third_party/winsdk_samples/src":
      (Var("googlecode_url") % "webrtc") + "/deps/third_party/winsdk_samples_v71@3145",

    # Used by libjpeg-turbo.
    "third_party/yasm/binaries":
      From("chromium_deps", "src/third_party/yasm/binaries"),
  },
  "unix": {
    "third_party/gold":
      From("chromium_deps", "src/third_party/gold"),
  },
  "android": {
    "third_party/android_tools":
      From("chromium_deps", "src/third_party/android_tools"),

    "third_party/android_testrunner":
      Var("chromium_trunk") + "/src/third_party/android_testrunner@" + Var("chromium_revision"),
  },
}

hooks = [
  {
    # Create a supplement.gypi file under trunk/webrtc. This file will be picked
    # up by gyp and used to enable the standalone build.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/tools/create_supplement_gypi.py",
               Var("root_dir") + "/webrtc/supplement.gypi"],
  },
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/tools/clang/scripts/update.py",
               "--mac-only"],
  },
  {
    # Update the cygwin mount on Windows.
    # This is necessary to get the correct mapping between e.g. /bin and the
    # cygwin path on Windows. Without it we can't run bash scripts in actions.
    # Ideally this should be solved in "pylib/gyp/msvs_emulation.py".
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/build/win/setup_cygwin_mount.py",
               "--win-only"],
  },
  {
    # Download test resources, i.e. video and audio files. If the latest
    # version is already downloaded, this takes zero seconds to run.
    # If a newer version or no current download exists, it will download
    # the resources and extract them.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/tools/resources/update.py"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/build/gyp_chromium",
               "--depth=" + Var("root_dir"), Var("root_dir") + "/webrtc.gyp",
               Var("extra_gyp_flag")],
  },
]

