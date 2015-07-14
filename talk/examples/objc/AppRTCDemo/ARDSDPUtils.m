/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ARDSDPUtils.h"

#import "ARDLogging.h"
#import "RTCSessionDescription.h"

@implementation ARDSDPUtils

+ (RTCSessionDescription *)
    descriptionForDescription:(RTCSessionDescription *)description
          preferredVideoCodec:(NSString *)codec {
  NSString *sdpString = description.description;
  NSString *lineSeparator = @"\n";
  NSString *mLineSeparator = @" ";
  // Copied from PeerConnectionClient.java.
  // TODO(tkchin): Move this to a shared C++ file.
  NSMutableArray *lines =
      [NSMutableArray arrayWithArray:
          [sdpString componentsSeparatedByString:lineSeparator]];
  NSInteger mLineIndex = -1;
  NSString *codecRtpMap = nil;
  // a=rtpmap:<payload type> <encoding name>/<clock rate>
  // [/<encoding parameters>]
  NSString *pattern =
      [NSString stringWithFormat:@"^a=rtpmap:(\\d+) %@(/\\d+)+[\r]?$", codec];
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:nil];
  for (NSInteger i = 0; (i < lines.count) && (mLineIndex == -1 || !codecRtpMap);
       ++i) {
    NSString *line = lines[i];
    if ([line hasPrefix:@"m=video"]) {
      mLineIndex = i;
      continue;
    }
    NSTextCheckingResult *codecMatches =
        [regex firstMatchInString:line
                          options:0
                            range:NSMakeRange(0, line.length)];
    if (codecMatches) {
      codecRtpMap =
          [line substringWithRange:[codecMatches rangeAtIndex:1]];
      continue;
    }
  }
  if (mLineIndex == -1) {
    ARDLog(@"No m=video line, so can't prefer %@", codec);
    return description;
  }
  if (!codecRtpMap) {
    ARDLog(@"No rtpmap for %@", codec);
    return description;
  }
  NSArray *origMLineParts =
      [lines[mLineIndex] componentsSeparatedByString:mLineSeparator];
  if (origMLineParts.count > 3) {
    NSMutableArray *newMLineParts =
        [NSMutableArray arrayWithCapacity:origMLineParts.count];
    NSInteger origPartIndex = 0;
    // Format is: m=<media> <port> <proto> <fmt> ...
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:codecRtpMap];
    for (; origPartIndex < origMLineParts.count; ++origPartIndex) {
      if (![codecRtpMap isEqualToString:origMLineParts[origPartIndex]]) {
        [newMLineParts addObject:origMLineParts[origPartIndex]];
      }
    }
    NSString *newMLine =
        [newMLineParts componentsJoinedByString:mLineSeparator];
    [lines replaceObjectAtIndex:mLineIndex
                     withObject:newMLine];
  } else {
    ARDLog(@"Wrong SDP media description format: %@", lines[mLineIndex]);
  }
  NSString *mangledSdpString = [lines componentsJoinedByString:lineSeparator];
  return [[RTCSessionDescription alloc] initWithType:description.type
                                                 sdp:mangledSdpString];
}

@end
