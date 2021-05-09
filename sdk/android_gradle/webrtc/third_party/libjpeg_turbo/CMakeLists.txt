cmake_minimum_required(VERSION 3.4.1)

file(GLOB_RECURSE src_jpeg_turbo
        ${WEBRTC_REPO}/third_party/libjpeg_turbo/*.S
        ${WEBRTC_REPO}/third_party/libjpeg_turbo/*.c
        )
list(FILTER src_jpeg_turbo EXCLUDE REGEX ${TEST_CC_FILTER})
list(FILTER src_jpeg_turbo EXCLUDE REGEX 
        ".*ext.c|.*arith.c|.*565.c|.*bmp.c|.*djpeg.c|.*jstdhuff.c|.*altivec.c|.*bench.c|.*turbojpeg.c|.*turbojpeg-jni.c")

add_library(jpeg_turbo STATIC
        ${src_jpeg_turbo}
        )
