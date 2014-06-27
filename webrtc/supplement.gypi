{
  'variables': {
    'variables': {
      'webrtc_root%': '<(DEPTH)/webrtc',
    },
    'webrtc_root%': '<(webrtc_root)',
    'build_with_chromium': 0,
    'use_sanitizer_options': 0,
  },
  'target_defaults': {
    'conditions': [
      # Add default sanitizer options similar to Chromium. This cannot be
      # put in webrtc/build/common.gypi since that file is not included by
      # third party code (yasm will throw leak errors during compile when
      # GYP_DEFINES="asan=1".
      ['OS=="linux" and (chromeos==0 or target_arch!="ia32")', {
        'dependencies': [
          '<(webrtc_root)/build/sanitizer_options.gyp:sanitizer_options',
        ],
      }],
    ],
  },
}
