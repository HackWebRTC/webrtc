#include "modules/transit_media/exposed_h264_bitstream_parser.h"

namespace webrtc {

int ExposedH264BitstreamParser::width() {
  if (!sps_) {
    return 640;
  }
  return sps_->width;
}

int ExposedH264BitstreamParser::height() {
  if (!sps_) {
    return 480;
  }
  return sps_->height;
}

}  // namespace webrtc
