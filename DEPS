# This file contains dependencies for WebRTC that are not shared with Chromium.
# If you wish to add a dependency that is present in Chromium's src/DEPS or a
# directory from the Chromium checkout, you should add it to setup_links.py
# instead.

vars = {
  'extra_gyp_flag': '-Dextra_gyp_flag=0',
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '24b4c7372a4e6a59c151909a5889906e57fb71c4',
}

# NOTE: Prefer revision numbers to tags for svn deps. Use http rather than
# https; the latter can cause problems for users behind proxies.
deps = {
  # When rolling gflags, also update
  # https://chromium.googlesource.com/chromium/deps/webrtc/webrtc.DEPS
  'src/third_party/gflags/src':
    Var('chromium_git') + '/external/gflags/src@e7390f9185c75f8d902c05ed7d20bb94eb914d0c', # from svn revision 82

  'src/third_party/junit':
    Var('chromium_git') + '/external/webrtc/deps/third_party/junit@f35596b476aa6e62fd3b3857b9942ddcd13ce35e', # from svn revision 3367
}

deps_os = {
  'win': {
    'src/third_party/winsdk_samples/src':
      Var('chromium_git') + '/external/webrtc/deps/third_party/winsdk_samples_v71@373e927dc5ffdb61b9fb63da3d261e71f8d50dc8', # from svn revision 3145
  },
}

# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  '-base',
  '-chromium',
  '+gflags',
  '+net',
  '+talk',
  '+testing',
  '+third_party',
  '+webrtc',
]

# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  'webrtc/overrides',
]

hooks = [
  {
    # Check for legacy named top-level dir (named 'trunk').
    'name': 'check_root_dir_name',
    'pattern': '.',
    'action': ['python','-c',
               ('import os,sys;'
                'script = os.path.join("trunk","check_root_dir.py");'
                '_ = os.system("%s %s" % (sys.executable,script)) '
                'if os.path.exists(script) else 0')],
  },
  {
    # Clone chromium and its deps.
    'name': 'sync chromium',
    'pattern': '.',
    'action': ['python', '-u', 'src/sync_chromium.py',
               '--target-revision', Var('chromium_revision')],
  },
  {
    # Create links to shared dependencies in Chromium.
    'name': 'setup_links',
    'pattern': '.',
    'action': ['python', 'src/setup_links.py'],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--no_auth',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'name': 'gyp',
    'pattern': '.',
    'action': ['python', 'src/webrtc/build/gyp_webrtc',
               Var('extra_gyp_flag')],
  },
]

