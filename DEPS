# This file contains dependencies for WebRTC.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'webrtc_git': 'https://webrtc.googlesource.com',
  'chromium_revision': '8158f07c248883ba3928996a567df957581e1e89',
  'boringssl_git': 'https://boringssl.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': '7a61cf37d6a0163f0ec02d495289a1d038e62457',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': '3390fd88d716ea599d659c2b72b9a6cd4bb36442',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': 'e6527b0cd469e3ff5764785dadcb39bf7d787154',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'eb73833a2935a42bb618d0557e3d45ae6624c9e9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'e847d8a9b47158695593d5693b0f69250472b229',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '31757f969fba60d75404f31e8f1168bef5011770',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling HarfBuzz
  # and whatever else without interference from each other.
  'harfbuzz_revision': '4f37ab63de9705d7bf74ee75364747e41b7c06a1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling luci-go CIPD package version
  # and whatever else without interference from each other.
  'luci_go': 'git_revision:25958d48e89e980e2a97daeddc977fb5e2e1fb8c',
}
deps = {
  # TODO(kjellander): Move this to be Android-only once the libevent dependency
  # in base/third_party/libevent is solved.
  'src/base':
    Var('chromium_git') + '/chromium/src/base' + '@' + '1b069d3e88232f1b9af86b4c38435fca2f9b631c',
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + 'ac2f1f6da8066776ff96a32aba51a67b3ac84fa3',
  'src/buildtools':
    Var('chromium_git') + '/chromium/src/buildtools' + '@' + '794f2d1f1c186f9a6ed6b543be810aca0c916f1d',
  # Gradle 4.3-rc4. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': Var('chromium_git') + '/external/github.com/gradle/gradle.git' + '@' +
      '89af43c4d0506f69980f00dde78c97b2f81437f8',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': Var('chromium_git') + '/chromium/src/ios' + '@' + '2617208b4de6a2563b58692bf990a63426073690',
    'condition': 'checkout_ios',
  },
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + 'd9c8fc7872f544adf41f069d6f8f75a98e93b203',
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + 'bc7297d1007a722ff8f874bdc6b9c18970e86d9e',

  'src/buildtools/clang_format/script':
    Var('chromium_git') + '/chromium/llvm-project/cfe/tools/clang-format.git' + '@' + '96636aa0e9f047f17447f2d45a094d0b59ed7917',
  'src/buildtools/third_party/libc++/trunk':
    Var('chromium_git') + '/chromium/llvm-project/libcxx.git' + '@' + 'a50f5035629b7621e92acef968403f71b7d48553',
  'src/buildtools/third_party/libc++abi/trunk':
    Var('chromium_git') + '/chromium/llvm-project/libcxxabi.git' + '@' + '0d529660e32d77d9111912d73f2c74fc5fa2a858',
  'src/buildtools/third_party/libunwind/trunk':
    Var('chromium_git') + '/external/llvm.org/libunwind.git' + '@' + '69d9b84cca8354117b9fe9705a4430d789ee599b',

  'src/tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'kykIT8m8YzNqqLP2xFGBTuo0ZtU9lom3BwiStWleyWkC',
      }
    ],
    'condition': 'checkout_mac',
    'dep_type': 'cipd',
  },

  'src/third_party/android_ndk': {
      'url': Var('chromium_git') + '/android_ndk.git' + '@' + '4e2cea441bfd43f0863d14f57b1e1844260b9884',
      'condition': 'checkout_android',
  },
  'src/third_party/android_tools': {
    'url': Var('chromium_git') + '/android_tools.git' + '@' + '347a7c8078a009e98995985b7ab6ec6b35696dea',
    'condition': 'checkout_android',
  },

  'src/third_party/android_build_tools/aapt2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'XPNW95mgY7ws_5lNsyjlq7DowuughMNsRIGuGCT0basC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bundletool': {
      'packages': [
          {
              'package': 'chromium/third_party/android_tools_bundletool',
              'version': 'GpDcZ4f-UcI4u6Kk7Ud-vtiLMltFr8vW63-NThsB5tEC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
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
    'url': Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + 'ba412eaaacd3186085babcd901679a48863c7dd5',
    'condition': 'checkout_android',
  },
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '77e5d48a085ee4fe7f6e10f5dcbb12fbc59eb4d2',
  'src/third_party/errorprone/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/errorprone.git' + '@' + '980d49e839aa4984015efed34b0134d4b2c9b6d7',
      'condition': 'checkout_android',
  },
  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + '7e1e8a4f7df474a4f8109c507a09621acad40314',
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
    Var('chromium_git') + '/external/github.com/google/gtest-parallel' + '@' + '3ca6798e2c2a06708888611bc5147bd1266f97a0',
  'src/third_party/google-truth': {
      'packages': [
          {
              'package': 'chromium/third_party/google-truth',
              'version': '4d6fe892fc3150ab40ef1d619baf0038859eb6d2',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + 'efecb0bfa687cf87836494f5d62868485c00fb66',
  'src/third_party/icu': {
    'url': Var('chromium_git') + '/chromium/deps/icu.git' + '@' + '8c67416ccb4da42d817e7081ff83a2193b1aabe7',
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
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '61a2bbaa9aec89cb2c882d87ace6aba9aee49bb9',
  'src/third_party/libsrtp':
    Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + '650611720ecc23e0e6b32b0e3100f8b4df91696c',
  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  '8256c8b297c8b7c7ee4de24edff82ed67d6ef207',
  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + 'b36c86fdfe746d7be904c3a565b047b24d58087e',
  'src/third_party/lss': {
    'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': Var('chromium_git') + '/external/mockito/mockito.git' + '@' + '04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },

  # Used by boringssl.
  'src/third_party/nasm': {
      'url': Var('chromium_git') + '/chromium/deps/nasm.git' + '@' +
      '4ee6a69ce33be1e96fd3c44a6e3ae3d8177453da'
  },

  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '6f26bce0b1c4e8ce0e13332f7c0083788def5fdf',
  'src/third_party/r8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'SlcbUnEufAQ-iuOwGOl8yYQuctmpf7bMqh59kBfpil0C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/proguard': {
      'packages': [
          {
              'package': 'chromium/third_party/proguard',
              'version': '3bd778c422ea5496de2ef25c007a517dbb5ce5ca',
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
    'url': Var('chromium_git') + '/external/robolectric.git' + '@' + '3d6dcabf5521e028c8efc2778ab6bd8c7b6d923c',
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
    'url': Var('webrtc_git') + '/deps/third_party/winsdk_samples_v71' + '@' + 'a9ac2ba82fd54721ec3c229593bcb524beea81a7',
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
    Var('chromium_git') + '/chromium/src/tools' + '@' + '848a110b95ab4fd3a9be7441bf3891cab5e94688',
  'src/tools/swarming_client':
    Var('chromium_git') + '/infra/luci/client-py.git' + '@' +  Var('swarming_revision'),

  'src/third_party/accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/accessibility-test-framework',
              'version': 'b5ec1e56e58e56bc1a0c77d43111c37f9b512c8a',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_support_test_runner': {
      'packages': [
          {
              'package': 'chromium/third_party/android_support_test_runner',
              'version': '96d4bf848cd210fdcbca6bcc8c1b4b39cbd93141',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/apk-patch-size-estimator': {
      'packages': [
          {
              'package': 'chromium/third_party/apk-patch-size-estimator',
              'version': 'b603e99dca9b90d6a99519c232cd811878283b08',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/bazel': {
      'packages': [
          {
              'package': 'chromium/third_party/bazel',
              'version': '1794576f65a721eb0af320a0701e48d31f1b2415',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/bouncycastle': {
      'packages': [
          {
              'package': 'chromium/third_party/bouncycastle',
              'version': 'c078e87552ba26e776566fdaf0f22cd8712743d0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/byte_buddy',
              'version': 'c9b53316603fc2d997c899c7ca1707f809b918cd',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/espresso': {
      'packages': [
          {
              'package': 'chromium/third_party/espresso',
              'version': 'c92dcfc4e894555a0b3c309f2b7939640eb1fee4',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/gson': {
      'packages': [
          {
              'package': 'chromium/third_party/gson',
              'version': '681931c9778045903a0ed59856ce2dd8dd7bf7ca',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/guava': {
      'packages': [
          {
              'package': 'chromium/third_party/guava',
              'version': 'a6fba501f3a0de88b9be1daa2052632de5b96a46',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/hamcrest': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': '37eccfc658fe79695d6abb6dd497463c4372032f',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/icu4j': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': 'e87e5bed2b4935913ee26a3ebd0b723ee2344354',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/intellij': {
      'packages': [
          {
              'package': 'chromium/third_party/intellij',
              'version': '77c2721b024b36ee073402c08e6d8428c0295336',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/objenesis',
              'version': '9e367f55e5a65781ee77bfcbaa88fb82b30e75c0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/ow2_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/ow2_asm',
              'version': 'NNAhdJzMdnutUVqfSJm5v0tVazA9l3Dd6CRwH6N4Q5kC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric',
              'version': 'iRFT1e5YFmRn5cbV0cAkQ5vDUXFmQ4qPYqStmmDfiMMC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/androidx': {
      'packages': [
          {
              'package': 'chromium/third_party/androidx',
              'version': 'BgU0HKOH7unGo87kXkIKJlPMmaSOCFhvUKcIr9aborwC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/sqlite4java': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': '889660698187baa7c8b0d79f7bf58563125fbd66',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/xstream': {
      'packages': [
          {
              'package': 'chromium/third_party/xstream',
              'version': '4278b1b78b86ab7a1a29e64d5aec9a47a9aab0fe',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': Var('luci_go'),
        },
        {
          'package': 'infra/tools/luci/isolated/${{platform}}',
          'version': Var('luci_go'),
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': Var('luci_go'),
        },
      ],
      'dep_type': 'cipd',
  },

  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //tools/android/roll/android_deps/fetch_all.py
  'src/third_party/android_deps/libs/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_common',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_runtime',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_annotation_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_annotation_annotation',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common',
              'version': 'version:2.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_core',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_ext_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_ext_junit',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_monitor': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_monitor',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_animated_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_animated_vector_drawable',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_appcompat_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_appcompat_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_cardview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cardview_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_design': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_design',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_gridlayout_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_gridlayout_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_leanback_v17',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_mediarouter_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_mediarouter_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_multidex',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_palette_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_palette_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_leanback_v17',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_v14': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_v14',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_recyclerview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_recyclerview_v7',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_compat',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_ui',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_utils',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_fragment',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_media_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_media_compat',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_v13': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v13',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v4',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_vector_drawable',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_transition',
              'version': 'version:27.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:16.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:16.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_fido': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_fido',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:16.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:15.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_ar_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_ar_core',
              'version': 'version:1.6.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:1.3.9-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2.17-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_compiler': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_compiler',
              'version': 'version:2.17-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_producers': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_producers',
              'version': 'version:2.17-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_spi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_spi',
              'version': 'version:2.17-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2.1.3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_javac_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac_shaded',
              'version': 'version:9-dev-r4023-3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format',
              'version': 'version:1.5-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:25.0-jre-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_protobuf_protobuf_lite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_lite',
              'version': 'version:3.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:1.11.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2.3.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:1.14-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===
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
]

recursedeps = []

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

  "+WebRTC",
  "+api",
  "+modules/include",
  "+rtc_base",
  "+test",
  "+rtc_tools",

  # Abseil whitelist. Keep this in sync with abseil-in-webrtc.md.
  "+absl/algorithm/algorithm.h",
  "+absl/algorithm/container.h",
  "+absl/base/attributes.h",
  "+absl/base/config.h",
  "+absl/container/inlined_vector.h",
  "+absl/memory/memory.h",
  "+absl/meta/type_traits.h",
  "+absl/strings/ascii.h",
  "+absl/strings/match.h",
  "+absl/strings/str_replace.h",
  "+absl/strings/string_view.h",
  "+absl/types/optional.h",
  "+absl/types/variant.h",
]
