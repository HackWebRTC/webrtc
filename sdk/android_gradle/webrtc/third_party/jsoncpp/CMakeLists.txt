cmake_minimum_required(VERSION 3.4.1)

include_directories(
    ${WEBRTC_REPO}/third_party/jsoncpp/source/include
)

file(GLOB_RECURSE src_json
    ${WEBRTC_REPO}/third_party/jsoncpp/source/src/lib_json/*.cpp
)
list(FILTER src_json EXCLUDE REGEX ${TEST_CC_FILTER})

add_library(json STATIC
    ${src_json}
)
