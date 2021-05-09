cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR -DSCTP_USE_OPENSSL_SHA1 -D__Userspace__ -D__Userspace_os_Linux -D_GNU_SOURCE -DNON_WINDOWS_DEFINE)

include_directories(
    ${WEBRTC_REPO}/third_party/boringssl/src/include
    ${WEBRTC_REPO}/third_party/usrsctp/usrsctplib/usrsctplib
)

file(GLOB_RECURSE src_usrsctp
    ${WEBRTC_REPO}/third_party/usrsctp/usrsctplib/usrsctplib/*.c
)
list(FILTER src_usrsctp EXCLUDE REGEX ${TEST_CC_FILTER})

add_library(usrsctp STATIC
    ${src_usrsctp}
)
