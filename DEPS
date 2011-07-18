vars = {
  "webrtc_trunk" : "https://webrtc.googlecode.com/svn/trunk",
  "chromium_trunk" : "http://src.chromium.org/svn/trunk",
  "chromium_revision": "86252",
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "libjingle_revision": "59",
}

deps = {
  "trunk/build":
    Var("chromium_trunk") + "/src/build@" + Var("chromium_revision"),

  "trunk/testing":
    Var("chromium_trunk") + "/src/testing@" + Var("chromium_revision"),

  "trunk/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@539",

  "trunk/tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@930",

  "trunk/third_party/protobuf/":
    Var("chromium_trunk") + "/src/third_party/protobuf@" + Var("chromium_revision"),

  "trunk/third_party/libvpx/source/libvpx":
    "git://review.webmproject.org/libvpx.git@v0.9.6",

  "trunk/third_party/libjpeg_turbo/":
    Var("chromium_trunk") + "/deps/third_party/libjpeg_turbo@78340",

  "trunk/third_party/libjpeg/":
    Var("chromium_trunk") + "/src/third_party/libjpeg@" + Var("chromium_revision"),

  "trunk/third_party/yasm/":
    Var("chromium_trunk") + "/src/third_party/yasm@" + Var("chromium_revision"),

  "trunk/third_party/expat/":
    Var("chromium_trunk") + "/src/third_party/expat@" + Var("chromium_revision"),

  "trunk/third_party/libjingle/":
    Var("chromium_trunk") + "/src/third_party/libjingle@" + Var("chromium_revision"),

  "trunk/third_party/libjingle/source":
    (Var("googlecode_url") % "libjingle") + "/branches/chrome-sandbox@" + Var("libjingle_revision"),

  "trunk/third_party/yasm/source/patched-yasm":
    Var("chromium_trunk") + "/deps/third_party/yasm/patched-yasm@73761",
  # Used by libjpeg-turbo
  "trunk/third_party/yasm/binaries":
    Var("chromium_trunk") + "/deps/third_party/yasm/binaries@74228",

  "trunk/third_party/jsoncpp/":
    "https://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/tags/jsoncpp/0.5.0",
}

deps_os = {
  "win": {
    "trunk/third_party/cygwin/":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",
  }
}

hooks = [
  {
    "pattern": ".",
    "action": ["svn", "export", Var("webrtc_trunk") + "/third_party_mods/libjingle", "trunk/third_party/libjingle", "--force"],
  },
  {
    "pattern": ".",
    "action": ["svn", "export", Var("webrtc_trunk") + "/third_party_mods/jsoncpp", "trunk/third_party/jsoncpp", "--force"],
  },
  {
    # Create a supplement.gypi file under trunk/.  This file will be picked up
    # by gyp and we use it to set Chromium related variables (inside_chromium_build)
    # to 0 and enable the standalone build.
    "pattern": ".",
    "action": ["python", "trunk/tools/create_supplement_gypi.py", "trunk/src/supplement.gypi"],
  },
  # A change to a .gyp, .gypi, or to GYP itself should run the generator.
  {
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_chromium", "--depth=trunk", "trunk/webrtc.gyp"],
  },
]

