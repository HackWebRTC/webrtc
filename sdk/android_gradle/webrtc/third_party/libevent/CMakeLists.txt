cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DHAVE_CONFIG_H=1)

include_directories(
    ${WEBRTC_REPO}

    ${WEBRTC_REPO}/base/third_party/libevent
    ${WEBRTC_REPO}/base/third_party/libevent/android
)

# infered from base/third_party/libevent/BUILD.gn, the base source set, plus Android source (epoll.c)
set(src_event
    ${WEBRTC_REPO}/base/third_party/libevent/buffer.c
    ${WEBRTC_REPO}/base/third_party/libevent/evbuffer.c
    ${WEBRTC_REPO}/base/third_party/libevent/evdns.c
    ${WEBRTC_REPO}/base/third_party/libevent/event.c
    ${WEBRTC_REPO}/base/third_party/libevent/event_tagging.c
    ${WEBRTC_REPO}/base/third_party/libevent/evrpc.c
    ${WEBRTC_REPO}/base/third_party/libevent/evutil.c
    ${WEBRTC_REPO}/base/third_party/libevent/http.c
    ${WEBRTC_REPO}/base/third_party/libevent/log.c
    ${WEBRTC_REPO}/base/third_party/libevent/poll.c
    ${WEBRTC_REPO}/base/third_party/libevent/select.c
    ${WEBRTC_REPO}/base/third_party/libevent/signal.c
    ${WEBRTC_REPO}/base/third_party/libevent/strlcpy.c

    ${WEBRTC_REPO}/base/third_party/libevent/epoll.c
)

add_library(event STATIC
    ${src_event}
)
