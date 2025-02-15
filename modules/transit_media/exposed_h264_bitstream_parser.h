#pragma once

#include "common_video/h264/h264_bitstream_parser.h"

namespace webrtc {

class ExposedH264BitstreamParser : public H264BitstreamParser {
 public:
  int width();
  int height();
};

}  // namespace webrtc
