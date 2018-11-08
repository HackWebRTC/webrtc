cmake_minimum_required(VERSION 3.4.1)

include_directories(
    ${WEBRTC_REPO}/third_party/boringssl/src/include
    ${WEBRTC_REPO}/third_party/libsrtp/config
    ${WEBRTC_REPO}/third_party/libsrtp/crypto/include
    ${WEBRTC_REPO}/third_party/libsrtp/include
    ${WEBRTC_REPO}/third_party/protobuf/src
)

add_definitions(
    -DHAVE_CONFIG_H
    -DOPENSSL
    -DHAVE_STDLIB_H
    -DHAVE_STRING_H
    -DHAVE_STDINT_H
    -DHAVE_INTTYPES_H
    -DHAVE_INT16_T
    -DHAVE_INT32_T
    -DHAVE_INT8_T
    -DHAVE_UINT16_T
    -DHAVE_UINT32_T
    -DHAVE_UINT64_T
    -DHAVE_UINT8_T
    -DHAVE_ARPA_INET_H
    -DHAVE_NETINET_IN_H
    -DHAVE_SYS_TYPES_H
    -DHAVE_UNISTD_H
    -DPACKAGE_STRING="libsrtp2 2.1.0-pre"
    -DPACKAGE_VERSION="2.1.0-pre"
)

file(GLOB_RECURSE src_srtp
    ${WEBRTC_REPO}/third_party/libsrtp/*.c
)
list(FILTER src_srtp EXCLUDE REGEX ${TEST_CC_FILTER})

add_library(srtp STATIC
    ${src_srtp}
)
