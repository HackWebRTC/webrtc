cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DHAVE_PTHREAD)

include_directories(
    ${WEBRTC_REPO}/third_party/protobuf/src
)

# extracted from `protobuf_lite_sources` of third_party/protobuf/BUILD.gn
# python extract_src_from_gn.py third_party/protobuf/BUILD.gn protobuf_lite_sources '    ${WEBRTC_REPO}/third_party/protobuf/'
set(src_protobuf_lite
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/any_lite.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/arena.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/arenastring.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/extension_set.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/generated_enum_util.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/generated_message_table_driven_lite.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/generated_message_util.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/implicit_weak_message.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/coded_stream.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/io_win32.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/strtod.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/message_lite.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/repeated_field.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/bytestream.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/common.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/int128.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/status.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/statusor.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/stringpiece.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/stringprintf.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/structurally_valid.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/strutil.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/stubs/time.cc
    ${WEBRTC_REPO}/third_party/protobuf/src/google/protobuf/wire_format_lite.cc
)

add_library(protobuf_lite STATIC
    ${src_protobuf_lite}
)
