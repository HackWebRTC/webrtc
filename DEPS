# This file contains dependencies for WebRTC.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'webrtc_git': 'https://webrtc.googlesource.com',
  'chromium_revision': '961775510291644dac141cf54adbcbba297dd731',
  'boringssl_git': 'https://boringssl.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': '486c9b53c4d54dd4b95bb6ce0e31160e600dfc11',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': 'c7db3232c397aa3feb1d474d63a1c4dd674b6349',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': 'e6527b0cd469e3ff5764785dadcb39bf7d787154',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '42016a47fa2b5e8b2bdb90615385a67d16589499',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'd62662686b220de9364f9104621a2616605c3673',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': 'b532d7ce708cb5ca9bf88abaa2eb213ddcf9babb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling HarfBuzz
  # and whatever else without interference from each other.
  'harfbuzz_revision': '2b76767bf572364d3d647cdd139f2044a7ad06b2',
}
deps = {
  # TODO(kjellander): Move this to be Android-only once the libevent dependency
  # in base/third_party/libevent is solved.
  'src/base':
    Var('chromium_git') + '/chromium/src/base' + '@' + '6d0a05ba3b60e617fe839acebac0825e72cfd27b',
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + 'af6cc8217179268370225c2c52142ccc0cff2d28',
  'src/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + '691bfec9d73bfefae30bad32e7a6496f2beceb9c',
  # Gradle 4.3-rc4. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': Var('chromium_git') + '/external/github.com/gradle/gradle.git' + '@' +
      '89af43c4d0506f69980f00dde78c97b2f81437f8',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': Var('chromium_git') + '/chromium/src/ios' + '@' + 'b5ab4791b396a03cfa286230252415b2abb4b2fe',
    'condition': 'checkout_ios',
  },
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '8446067591b079c5bf31fef6d945f65cd1f046b0',
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + 'ae03b402bf242ddaa9eef96c14d83495ec5baeb1',
  'src/third_party/android_ndk': {
      'url': Var('chromium_git') + '/android_ndk.git' + '@' + '5cd86312e794bdf542a3685c6f10cbb96072990b',
      'condition': 'checkout_android',
  },
  'src/third_party/android_tools': {
    'url': Var('chromium_git') + '/android_tools.git' + '@' + '130499e25286f4d56acafa252fee09f3cc595c49',
    'condition': 'checkout_android',
  },
  'src/third_party/auto/src': {
    'url': Var('chromium_git') + '/external/github.com/google/auto.git' + '@' + '8a81a858ae7b78a1aef71ac3905fade0bbd64e82',
    'condition': 'checkout_android',
  },
  'src/third_party/boringssl/src':
    Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),
  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + Var('catapult_revision'),
  'src/third_party/ced/src': {
    'url': Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + '94c367a1fe3a13207f4b22604fcfd1d9f9ddf6d9',
    'condition': 'checkout_android',
  },
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '254538b955bd353e28da028e09c1f808367a4bf4',
  'src/third_party/errorprone/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/errorprone.git' + '@' + '980d49e839aa4984015efed34b0134d4b2c9b6d7',
      'condition': 'checkout_android',
  },
  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + '90210b5e10d3917567a3025e4853704bfefd8384',
  'src/third_party/findbugs': {
    'url': Var('chromium_git') + '/chromium/deps/findbugs.git' + '@' + '4275d9ac8610db6b1bc9a5e887f97e41b33fac67',
    'condition': 'checkout_android',
  },
  'src/third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + Var('freetype_revision'),
  'src/third_party/harfbuzz-ng/src':
    Var('chromium_git') + '/external/github.com/harfbuzz/harfbuzz.git' + '@' + Var('harfbuzz_revision'),
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/github.com/google/gtest-parallel' + '@' + 'cb3514a0858be0f66281d892e2242d1073fd75fe',
  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + 'ce468a17c434e4e79724396ee1b51d86bfc8a88b',
  'src/third_party/icu': {
    'url': Var('chromium_git') + '/chromium/deps/icu.git' + '@' + '297a4dd02b9d36c92ab9b4f121e433c9c3bc14f8',
  },
  'src/third_party/jsr-305/src': {
    'url': Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
    'condition': 'checkout_android',
  },
  'src/third_party/jsoncpp/source':
    Var('chromium_git') + '/external/github.com/open-source-parsers/jsoncpp.git' + '@' + 'f572e8e42e22cfcf5ab0aea26574f408943edfa4', # from svn 248
  'src/third_party/junit/src': {
    'url': Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
    'condition': 'checkout_android',
  },
  # Used for building libFuzzers (only supports Linux).
  'src/third_party/libFuzzer/src':
    Var('chromium_git') + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git' + '@' +  Var('libfuzzer_revision'),
  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + 'a1750dbc79a8792dde3d3f7d7d8ac28ba01ac9dd',
  'src/third_party/libsrtp':
    Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + 'fc2345089a6b3c5aca9ecd2e1941871a78a13e9c',
  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  '2c45cd174a9582909ee2a7ba9cdb3feb917840cf',
  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + '55f5d91f11f929c4c59c32621c3d5457cca3ab0b',
  'src/third_party/lss': {
    'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': Var('chromium_git') + '/external/mockito/mockito.git' + '@' + '04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '3b51f16a4a41df729f8d647f03e48c5f272911ff',
  'src/third_party/r8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'version:1.2.28-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/requests/src': {
    'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
    'condition': 'checkout_android',
  },
  'src/third_party/robolectric/robolectric': {
    'url': Var('chromium_git') + '/external/robolectric.git' + '@' + '7e067f1112e1502caa742f7be72d37b5678d3403',
    'condition': 'checkout_android',
  },
  'src/third_party/ub-uiautomator/lib': {
    'url': Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
    'condition': 'checkout_android',
  },
  'src/third_party/usrsctp/usrsctplib':
    Var('chromium_git') + '/external/github.com/sctplab/usrsctp' + '@' + '7a8bc9a90ca96634aa56ee712856d97f27d903f8',
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/winsdk_samples': {
    'url': Var('webrtc_git') + '/deps/third_party/winsdk_samples_v71' + '@' + 'a59391ef795986633735a1695caa97622a9bfd56',
    'condition': 'checkout_win',
  },
  # Dependency used by libjpeg-turbo.
  'src/third_party/yasm/binaries': {
    'url': Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',
    'condition': 'checkout_win',
  },
  'src/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '720b70524a4424b15fc57e82263568c8ba0496ad',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + 'e622dcecee96b861b7ac17f2d0002c04a7751a0f',
  'src/tools/swarming_client':
    Var('chromium_git') + '/infra/luci/client-py.git' + '@' +  Var('swarming_revision'),

  'src/third_party/accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/accessibility-test-framework',
              'version': 'version:2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_support_test_runner': {
      'packages': [
          {
              'package': 'chromium/third_party/android_support_test_runner',
              'version': 'version:0.5-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/apk-patch-size-estimator': {
      'packages': [
          {
              'package': 'chromium/third_party/apk-patch-size-estimator',
              'version': 'version:0.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/bazel': {
      'packages': [
          {
              'package': 'chromium/third_party/bazel',
              'version': 'version:0.10.0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/bouncycastle': {
      'packages': [
          {
              'package': 'chromium/third_party/bouncycastle',
              'version': 'version:1.46-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/byte_buddy',
              'version': 'version:1.8.8-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/espresso': {
      'packages': [
          {
              'package': 'chromium/third_party/espresso',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/gson': {
      'packages': [
          {
              'package': 'chromium/third_party/gson',
              'version': 'version:2.8.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/guava': {
      'packages': [
          {
              'package': 'chromium/third_party/guava',
              'version': 'version:23.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/hamcrest': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': 'version:1.3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/icu4j': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': 'version:53.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/intellij': {
      'packages': [
          {
              'package': 'chromium/third_party/intellij',
              'version': 'version:12.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/javax_inject',
              'version': 'version:1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/objenesis',
              'version': 'version:2.4-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/ow2_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/ow2_asm',
              'version': 'version:5.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric',
              'version': 'version:3.5.1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/sqlite4java': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': 'version:0.282-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/xstream': {
      'packages': [
          {
              'package': 'chromium/third_party/xstream',
              'version': 'version:1.4.8-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Start ===
  'src/third_party/android_deps/repository/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/android_arch_core_common',
              'version': 'version:1.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/android_arch_lifecycle_common',
              'version': 'version:1.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/android_arch_lifecycle_runtime',
              'version': 'version:1.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_animated_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_animated_vector_drawable',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_appcompat_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_appcompat_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_cardview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_cardview_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_design': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_design',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_gridlayout_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_gridlayout_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_leanback_v17',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_mediarouter_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_mediarouter_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_multidex',
              'version': 'version:1.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_palette_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_palette_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_preference_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_preference_leanback_v17',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_preference_v14': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_preference_v14',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_preference_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_preference_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_recyclerview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_recyclerview_v7',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_annotations',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_compat',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_core_ui',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_core_utils',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_fragment',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_media_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_media_compat',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_v13': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_v13',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_v4',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_support_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_support_vector_drawable',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/repository/com_android_support_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/repository/com_android_support_transition',
              'version': 'version:27.0.0-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  # === ANDROID_DEPS End ===
}

hooks = [
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
        'src/tools_webrtc/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python',
        'src/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    # TODO(mbonadei): change to --arch=x86.
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=i386'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    # TODO(mbonadei): change to --arch=mips.
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mipsel'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    # TODO(mbonadei): change to --arch=x64.
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=amd64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'src/build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary. Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python', 'src/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python', 'src/build/mac_toolchain.py'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'condition': 'host_os == "linux"',
    'action': [
        'python',
        'src/third_party/binutils/download.py',
    ],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'src/tools/clang/scripts/update.py'],
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
    'condition': 'host_os == "win"',
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
    'condition': 'host_os == "mac"',
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
    'condition': 'host_os == "linux"',
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
    'condition': 'host_os == "win"',
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
    'condition': 'host_os == "mac"',
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
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/linux64/rc.sha1',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
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
    'condition': 'host_os == "mac"',
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
    'condition': 'host_os == "linux"',
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
    'condition': 'host_os == "win"',
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=a8456d9248a126881dcfb8707ca7dcdae56e1ac7',
               '--overwrite',
    ],
  },
  {
    'name': 'msan_chained_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-chained-origins-trusty.tgz.sha1",
              ],
  },
  {
    'name': 'msan_no_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-no-origins-trusty.tgz.sha1",
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
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    'condition': 'checkout_android',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python',
               'src/build/android/play_services/update.py',
               'download'
    ],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi.
  'src/buildtools',
  # android_tools manages the NDK.
  'src/third_party/android_tools',
]

# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  "+external/webrtc/webrtc",  # Android platform build.
  "+libyuv",

  # These should eventually move out of here.
  "+common_types.h",
  "+typedefs.h",

  "+WebRTC",
  "+api",
  "+modules/include",
  "+rtc_base",
  "+test",
  "+rtc_tools",

  # Abseil whitelist.
  "+absl/memory/memory.h",
  "+absl/types/optional.h",
  "+absl/types/variant.h",
]
