# WebRTC Android Studio project

A reference gradle project that let you explore WebRTC Android in Android Studio.

## Debug native code in Android Studio

Edit `gradle.properties`, set `compile_native_code=true` and other variables according to your WebRTC checkout location, then enjoy :)

Note:

+ you need download and sync WebRTC repo by yourself, this project won't do that for you;
+ use the same version of Android SDK and NDK as WebRTC does;
+ (re)create `protoc` after updating webrtc repo, build WebRTC with ninja would create it;
+ delete `webrtc_build_dir` after updating webrtc repo;
