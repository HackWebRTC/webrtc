cmake_minimum_required(VERSION 3.4.1)

include_directories(
    ${WEBRTC_REPO}
)

file(GLOB_RECURSE src_rnnoise
    ${WEBRTC_REPO}/third_party/rnnoise/src/*.cc
)
list(FILTER src_rnnoise EXCLUDE REGEX ${TEST_CC_FILTER})

add_library(rnnoise STATIC
    ${src_rnnoise}
)
