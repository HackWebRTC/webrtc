# This file contains dependencies for WebRTC.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '7d5e4125a792e09524a5942e829f2cfe738aa253',
  'boringssl_git': 'https://boringssl.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': '11e31afa5d330756ff87aa12064bb5d032896cb5',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  'openmax_dl_revision': '7acede9c039ea5d14cf326f44aad1245b9e674a7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': 'd04ca95356c5b79e9ad80f3e8d1bd9784213dc93',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '3f6478ac95edf86cd3da300c2c0d34a438f5dbeb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'ae1fc136b4a38b0328e546e29764eb96dbf85203',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'eebc6eb55995a29173f269baf071046fcd2e0f4b',
}

deps = {
  # TODO(kjellander): Move this to be Android-only once the libevent dependency
  # in base/third_party/libevent is solved.
  'src/base':
    Var('chromium_git') + '/chromium/src/base' + '@' + 'e26433ced07bd6502dec9673be7db4e95cea1e0e',
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + '3c5003eba85adc21ad5e73455db4f4987d04fbc4',
  'src/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + '95345544a05894b57398f60c16f6bd73eeb14eda',
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '36378edb3115fb605b0b14ed074ce50ce39edede',
  'src/testing/gtest':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '6f8a66431cb592dad629028a50b3dd418a408c87',
  'src/testing/gmock':
    Var('chromium_git') + '/external/googlemock.git' + '@' + '0421b6f358139f02e102c9c332ce19a33faf75be', # from svn revision 566
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + 'edffbd12b696f779dc3ca9566bb619c8143268be',
  'src/third_party/boringssl/src':
   Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),
  'src/third_party/catapult':
   Var('chromium_git') + '/external/github.com/catapult-project/catapult.git' + '@' + Var('catapult_revision'),
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + '3f3ad2ea90df08f5907bd997e1ce22e1c19ce215',
  'src/third_party/jsoncpp/source':
    Var('chromium_git') + '/external/github.com/open-source-parsers/jsoncpp.git' + '@' + 'f572e8e42e22cfcf5ab0aea26574f408943edfa4', # from svn 248

  # Used for building libFuzzers (only supports Linux).
  'src/third_party/libFuzzer/src':
    Var('chromium_git') + '/chromium/llvm-project/llvm/lib/Fuzzer.git' + '@' +  Var('libfuzzer_revision'),

  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '7260e4d8b8e1e40b17f03fafdf1cd83296900f76',
  'src/third_party/libsrtp':
   Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + '0e0936f3013fe5884eac82f95e370c8d460a179f',
  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  '8121f85473b28183c93fdcef290ed6f74b0b52db',
  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + '0741a3d70400dc96e59726674b0acf3bca02d710',
  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '0fd88df93c5dcaf858c57eb7892bd27763f0f0ac',
  'src/third_party/openmax_dl':
    Var('chromium_git') + '/external/webrtc/deps/third_party/openmax.git' + '@' +  Var('openmax_dl_revision'),
  'src/third_party/usrsctp/usrsctplib':
    Var('chromium_git') + '/external/github.com/sctplab/usrsctp' + '@' + '8679f2b0bf063ac894dc473debefd61dbbebf622',
  'src/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '7da28c6c7c6a1387217352ce02b31754deb54d2a',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + 'c19a99cef35076d369c7380bb86524dc85a4073d',
  'src/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'e7079f0e0e14108ab0dba58728ff219637458563',
   'src/tools/swarming_client':
     Var('chromium_git') + '/external/swarming.client.git' + '@' +  Var('swarming_revision'),

  # WebRTC-only dependencies (not present in Chromium).
  'src/third_party/gflags':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gflags' + '@' + '892576179b45861b53e04a112996a738309cf364',
  'src/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags' + '@' + '03bebcb065c83beff83d50ae025a55a4bf94dfca',
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gtest-parallel' + '@' + '1dad0e9f6d82ff994130b529d7d814b40eb32b0e',
}

deps_os = {
  'android': {
    'src/third_party/android_tools':
      Var('chromium_git') + '/android_tools.git' + '@' + 'b43a6a289a7588b1769814f04dd6c7d7176974cc',
    'src/third_party/ced/src':
      Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + 'e21eb6aed10b9f6e2727f136c52420033214d458',
    'src/third_party/icu':
      Var('chromium_git') + '/chromium/deps/icu.git' + '@' + '450be73c9ee8ae29d43d4fdc82febb2a5f62bfb5',
    'src/third_party/jsr-305/src':
      Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
    'src/third_party/junit/src':
      Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
    'src/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
    'src/third_party/mockito/src':
      Var('chromium_git') + '/external/mockito/mockito.git' + '@' + 'de83ad4598ad4cf5ea53c69a8a8053780b04b850',
    'src/third_party/requests/src':
      Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
    'src/third_party/robolectric/robolectric':
      Var('chromium_git') + '/external/robolectric.git' + '@' + 'e38b49a12fdfa17a94f0382cc8ffaf69132fd09b',
    'src/third_party/ub-uiautomator/lib':
      Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
  },
  'ios': {
    'src/ios':
      Var('chromium_git') + '/chromium/src/ios' + '@' + '1ef577e65b462c02d00c07b2d73e9bbf01188fed',
  },
  'unix': {
    'src/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
  },
  'win': {
    # Dependencies used by libjpeg-turbo
    'src/third_party/yasm/binaries':
      Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',

    # WebRTC-only dependency (not present in Chromium).
    'src/third_party/winsdk_samples':
      Var('chromium_git') + '/external/webrtc/deps/third_party/winsdk_samples_v71' + '@' + '2d31a1cbecc86359e6ec041fb9ff6c082babd073',
  },
}

pre_deps_hooks = [
  {
    # Remove any symlinks from before 177567c518b121731e507e9b9c4049c4dc96e4c8.
    # TODO(kjellander): Remove this in March 2017.
    'name': 'cleanup_links',
    'pattern': '.',
    'action': ['python', 'src/cleanup_links.py'],
  },
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
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/tools-webrtc/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  # Android dependencies. Many are downloaded using Google Storage these days.
  # They're copied from https://cs.chromium.org/chromium/src/DEPS for all
  # such dependencies we share with Chromium.
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python',
               'src/build/android/play_services/update.py',
               'download'
    ],
  },
  {
    'name': 'intellij',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-intellij',
               '-l', 'third_party/intellij'
    ],
  },
  {
    'name': 'javax_inject',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-javax-inject',
               '-l', 'third_party/javax_inject'
    ],
  },
  {
    'name': 'hamcrest',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-hamcrest',
               '-l', 'third_party/hamcrest'
    ],
  },
  {
    'name': 'guava',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-guava',
               '-l', 'third_party/guava'
    ],
  },
  {
    'name': 'android_support_test_runner',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-android-support-test-runner',
               '-l', 'third_party/android_support_test_runner'
    ],
  },
  {
    'name': 'byte_buddy',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-byte-buddy',
               '-l', 'third_party/byte_buddy'
    ],
  },
  {
    'name': 'espresso',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-espresso',
               '-l', 'third_party/espresso'
    ],
  },
  {
    'name': 'robolectric_libs',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-robolectric',
               '-l', 'third_party/robolectric'
    ],
  },
  {
    'name': 'apache_velocity',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-apache-velocity',
               '-l', 'third_party/apache_velocity'
    ],
  },
  {
    'name': 'ow2_asm',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-ow2-asm',
               '-l', 'third_party/ow2_asm'
    ],
  },
  {
    'name': 'icu4j',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-icu4j',
               '-l', 'third_party/icu4j'
    ],
  },
  {
    'name': 'accessibility_test_framework',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-accessibility-test-framework',
               '-l', 'third_party/accessibility_test_framework'
    ],
  },
  {
    'name': 'bouncycastle',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-bouncycastle',
               '-l', 'third_party/bouncycastle'
    ],
  },
  {
    'name': 'sqlite4java',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-sqlite4java',
               '-l', 'third_party/sqlite4java'
    ],
  },
  {
    'name': 'objenesis',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-objenesis',
               '-l', 'third_party/objenesis'
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'src/build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'action': [
        'python',
        'src/third_party/binutils/download.py',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'src/tools/clang/scripts/update.py', '--if-needed'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  # Pull GN binaries.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/linux64/gn.sha1',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/win64',
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/mac64',
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/linux64',
    ],
  },
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    'name': 'syzygy-binaries',
    'pattern': '.',
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=a8456d9248a126881dcfb8707ca7dcdae56e1ac7',
               '--overwrite',
    ],
  },
  {
    # Pull sanitizer-instrumented third-party libraries if requested via
    # GYP_DEFINES.
    # See src/third_party/instrumented_libraries/scripts/download_binaries.py.
    # TODO(kjellander): Update comment when GYP is completely cleaned up.
    'name': 'instrumented_libraries',
    'pattern': '\\.sha1',
    'action': ['python', 'src/third_party/instrumented_libraries/scripts/download_binaries.py'],
  },
  {
    'name': 'clang_format_merge_driver',
    'pattern': '.',
    'action': [ 'python',
                'src/tools/clang_format_merge_driver/install_git_hook.py',
    ],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--no_auth',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi.
  'src/buildtools',
  # android_tools manages the NDK.
  'src/third_party/android_tools',
]

