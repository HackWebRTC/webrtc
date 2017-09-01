# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  "+external/webrtc/webrtc",  # Android platform build.
  "+gflags",
  "+libyuv",
  "-webrtc",  # Has to be disabled; otherwise all dirs below will be allowed.
  # Individual headers that will be moved out of here, see webrtc:4243.
  "+webrtc/call/rtp_config.h",
  "+webrtc/common_types.h",
  "+webrtc/transport.h",
  "+webrtc/typedefs.h",
  "+webrtc/voice_engine_configurations.h",

  "+WebRTC",
  "+webrtc/api",
  "+webrtc/modules/include",
  "+webrtc/rtc_base",
  "+webrtc/test",
  "+webrtc/rtc_tools",
]

# The below rules will be removed when webrtc:4243 is fixed.
specific_include_rules = {
  "video_receive_stream\.h": [
    "+webrtc/call/video_receive_stream.h",
  ],
  "video_send_stream\.h": [
    "+webrtc/call/video_send_stream.h",
  ],
}
