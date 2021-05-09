cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DOPUS_BUILD -DOPUS_EXPORT= -DHAVE_LRINT -DHAVE_LRINTF -DVAR_ARRAYS -DOPUS_WILL_BE_SLOW -DFIXED_POINT)

include_directories(
    ${WEBRTC_REPO}/third_party/opus/src/celt
    ${WEBRTC_REPO}/third_party/opus/src/include
    ${WEBRTC_REPO}/third_party/opus/src/silk
    ${WEBRTC_REPO}/third_party/opus/src/silk/fixed
)

set(OPUS_SRC_FILTER ".*opus_custom_demo.c|.*mlp_train.c|.*opus_compare.c|.*opus_demo.c|.*repacketizer_demo.c")
file(GLOB src_opus_arm64
    ${WEBRTC_REPO}/third_party/opus/src/celt/*.c
    ${WEBRTC_REPO}/third_party/opus/src/silk/*.c
    ${WEBRTC_REPO}/third_party/opus/src/silk/fixed/*.c
    ${WEBRTC_REPO}/third_party/opus/src/src/*.c
)
list(FILTER src_opus_arm64 EXCLUDE REGEX ${OPUS_SRC_FILTER})

add_library(opus STATIC
    ${src_opus_arm64}
)
