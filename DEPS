# This file contains dependencies for WebRTC that are not shared with Chromium.
# If you wish to add a dependency that is present in Chromium's src/DEPS or a
# directory from the Chromium checkout, you should add it to setup_links.py
# instead.

vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  "root_dir": "trunk",
  "extra_gyp_flag": "-Dextra_gyp_flag=0",

  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "chromium_revision": "94532b1fa8a12155c3d15e2b91b1f3dc6b01cb0f",
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  # When rolling gflags, also update deps/third_party/webrtc/webrtc.DEPS/DEPS
  # in Chromium's repo.
  Var("root_dir") + "/third_party/gflags/src":
    (Var("googlecode_url") % "gflags") + "/trunk/src@84",

  Var("root_dir") + "/third_party/junit/":
    (Var("googlecode_url") % "webrtc") + "/deps/third_party/junit@3367",
}

deps_os = {
  "win": {
    Var("root_dir") + "/third_party/winsdk_samples/src":
      (Var("googlecode_url") % "webrtc") + "/deps/third_party/winsdk_samples_v71@3145",
  },
}

include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  '+net',
  '+talk',
  '+testing',
  '+webrtc',
]

# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  'gflags',
  'testing',
  'third_party',
]

hooks = [
  {
    # Clone chromium and its deps.
    "name": "sync chromium",
    "pattern": ".",
    "action": ["python", "-u", Var("root_dir") + "/sync_chromium.py",
               "--target-revision", Var("chromium_revision")],
  },
  {
    # Create links to shared dependencies in Chromium.
    "name": "setup_links",
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/setup_links.py"],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    "pattern": ".",
    "action": ["download_from_google_storage",
               "--directory",
               "--recursive",
               "--num_threads=10",
               "--no_auth",
               "--bucket", "chromium-webrtc-resources",
               Var("root_dir") + "/resources"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "name": "gyp",
    "pattern": ".",
    "action": ["python", Var("root_dir") + "/webrtc/build/gyp_webrtc",
               Var("extra_gyp_flag")],
  },
]

