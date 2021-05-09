# WebRTC Android Studio project

A reference gradle project that let you explore WebRTC Android in Android Studio.

## Debug native code in Android Studio

_break since #30771._

Edit `gradle.properties`, set `compile_native_code=true` and other variables according to your WebRTC checkout location, then enjoy :)

Note:

+ You need download and sync WebRTC repo by yourself, this project won't do that for you;
+ Checkout the same WebRTC commit as this project does, which is [#30987](https://webrtc.googlesource.com/src/+/04c1b445019e10e54b96f70403d25cc54215faf3);
+ Use the same version of Android SDK and NDK as WebRTC does;
+ (re)Create `protoc` after updating WebRTC repo, to create the `protoc` program, you need build WebRTC Android via ninja once, let's assume the output dir is `out/android_ninja`, then the `protoc` will be `out/android_ninja/clang_x64/protoc`;
+ Delete `webrtc_build_dir` after updating WebRTC repo;

## WebRTC src extractor

`python3 webrtc_src_extractor.py <repo dir> <dst dir> <wanted src file, seperated by space>`

If you only want use a small part of WebRTC code, this script could help you find all related sources and headers, and copy them into `dst dir`. Note that it's just a best effort script, you may still need copy some files manually.

## Caveat

+ Delete `webrtc_build_dir` and `.externalNativeBuild`, run `./gradlew genWebrtcSrc`, and "Refresh Linked C++ Projects" (note that "Sync Project with Gradle Files" won't work) before your build and debug, otherwise the generated sources may not be compiled, undefined reference error will happen, e.g. `webrtc::rtclog::Event` related references;
