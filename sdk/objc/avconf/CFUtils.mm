//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Piasy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//


#import "CFUtils.h"

#include "api/video_codecs/h264_profile_level_id.h"
#include "media/base/sdp_video_format_utils.h"

static std::string NSStringToStdString(NSString* s) {
    if (!s) {
        return "";
    }
    return std::string([s UTF8String]);
}

static std::map<std::string, std::string> NSDictionaryToStdMap(NSDictionary<NSString*, NSString*>* dict) {
    std::map<std::string, std::string> result;
    if (!dict) {
        return result;
    }
    for (NSString* key in dict) {
        result[NSStringToStdString(key)] = NSStringToStdString(dict[key]);
    }
    return result;
}

@implementation CFUtils

+ (bool)isSameH264Profile:(NSDictionary<NSString*, NSString*>*)aParams bParams:(NSDictionary<NSString*, NSString*>*)bParams {
    std::map<std::string, std::string> aMap = NSDictionaryToStdMap(aParams);
    std::map<std::string, std::string> bMap = NSDictionaryToStdMap(bParams);
    return webrtc::H264IsSameProfile(aMap, bMap);
}

+ (NSString*)H264GenerateProfileLevelIdForAnswer:(NSDictionary<NSString*, NSString*>*)aParams bParams:(NSDictionary<NSString*, NSString*>*)bParams {
    std::map<std::string, std::string> aMap = NSDictionaryToStdMap(aParams);
    std::map<std::string, std::string> bMap = NSDictionaryToStdMap(bParams);
    std::string profile_level_id;
    webrtc::CodecParameterMap newParams;
    webrtc::H264GenerateProfileLevelIdForAnswer(aMap, bMap, &newParams);
    auto profile_level_id_it = newParams.find("profile-level-id");
    if (profile_level_id_it != newParams.end()) {
        profile_level_id = profile_level_id_it->second;
    }
    return [NSString stringWithUTF8String:profile_level_id.c_str()];
}

+ (NSString*)modelName {
    return @"";
}

+ (NSString*)osVersion {
    return @"";
}

@end
