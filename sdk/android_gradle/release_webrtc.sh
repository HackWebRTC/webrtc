#!/bin/bash

./gradlew clean :webrtc:assembleDebug && \
cp webrtc/build/outputs/aar/webrtc-debug.aar ~/src/media/AvConf/libs/webrtc.aar
