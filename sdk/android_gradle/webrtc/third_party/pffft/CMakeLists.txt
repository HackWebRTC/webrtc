cmake_minimum_required(VERSION 3.4.1)

include_directories(
    ${WEBRTC_REPO}/third_party/pffft/src
)

file(GLOB src_pffft
    ${WEBRTC_REPO}/third_party/pffft/src/pffft.c
)

add_library(pffft STATIC
    ${src_pffft}
)
