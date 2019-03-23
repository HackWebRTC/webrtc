# WebRTC Android Studio project

A reference gradle project that let you explore WebRTC Android in Android Studio.

## Debug native code in Android Studio

Edit `gradle.properties`, set `compile_native_code=true` and other variables according to your WebRTC checkout location, then enjoy :)

Note:

+ You need download and sync WebRTC repo by yourself, this project won't do that for you;
+ Checkout the same WebRTC commit as this project does, which is [#27225](https://webrtc.googlesource.com/src/+/94b57c044e81c6d1938f60aeabe7115a373f626d);
+ Use the same version of Android SDK and NDK as WebRTC does;
+ (re)Create `protoc` after updating WebRTC repo, to create the `protoc` program, you need build WebRTC Android via ninja once, let's assume the output dir is `out/android_ninja`, then the `protoc` will be `out/android_ninja/clang_x64/protoc`;
+ Delete `webrtc_build_dir` after updating WebRTC repo;
