#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FFMPEG="${FFMPEG:-ffmpeg}"

common_video=(
  -c:v libx264
  -preset veryslow
  -tune zerolatency
  -profile:v baseline
  -pix_fmt yuv420p
  -x264-params threads=1:keyint=10:min-keyint=10:scenecut=0
)
common_output=(
  -movflags +faststart
  -map_metadata -1
  -fflags +bitexact
  -flags:v +bitexact
  -flags:a +bitexact
)

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.25" \
  -ac 1 -c:a aac -b:a 48k "${common_output[@]}" \
  "$SCRIPT_DIR/audio_only.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.30" \
  "${common_video[@]}" -an "${common_output[@]}" \
  "$SCRIPT_DIR/video_only.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.42" \
  -f lavfi -i "sine=frequency=660:sample_rate=48000:duration=0.42" \
  -map 0:v:0 -map 1:a:0 "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/audio_video.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.61" \
  -f lavfi -i "sine=frequency=550:sample_rate=32000:duration=0.31" \
  -map 0:v:0 -map 1:a:0 "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/audio_shorter.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.30" \
  -f lavfi -i "sine=frequency=770:sample_rate=48000:duration=0.61" \
  -map 0:v:0 -map 1:a:0 "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/video_shorter.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.42" \
  -f lavfi -i "sine=frequency=880:sample_rate=48000:duration=0.42" \
  -filter_complex "[0:v]setpts=PTS+0.25/TB[v];[1:a]asetpts=PTS+0.25/TB[a]" \
  -map "[v]" -map "[a]" "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/nonzero_start.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=30:duration=0.37" \
  -f lavfi -i "sine=frequency=990:sample_rate=48000:duration=0.347" \
  -map 0:v:0 -map 1:a:0 "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/non_aligned.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc=size=64x48:rate=10:duration=0.30" \
  -c:v libx264 -preset veryslow -profile:v high444 -pix_fmt yuv444p \
  -x264-params threads=1:keyint=10:min-keyint=10:scenecut=0 -an \
  "${common_output[@]}" "$SCRIPT_DIR/unsupported_yuv444.mp4"

# Keep the faststart metadata and initial decodable packets, then cut the media
# payload so Create succeeds and playback deterministically sees an early EOF.
dd if="$SCRIPT_DIR/audio_video.mp4" \
  of="$SCRIPT_DIR/truncated_audio_video.mp4" bs=1 count=6500 status=none

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.30" \
  -itsoffset 1.0 \
  -f lavfi -i "sine=frequency=330:sample_rate=48000:duration=0.30" \
  -map 0:v:0 -map 1:a:0 "${common_video[@]}" \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/no_common_interval.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.40" \
  -f lavfi -i "sine=frequency=220:sample_rate=48000:duration=2.80" \
  -filter_complex "[0:v]setpts=PTS+2.2/TB*gte(N\,2)[v]" \
  -map "[v]" -map 1:a:0 "${common_video[@]}" -fps_mode vfr \
  -ac 2 -c:a aac -b:a 64k "${common_output[@]}" \
  "$SCRIPT_DIR/gap_over_2s.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.25" \
  -f lavfi -i "sine=frequency=880:sample_rate=48000:duration=0.25" \
  -map 0:a:0 -map 1:a:0 -ac 2 -c:a aac -b:a 48k \
  "${common_output[@]}" "$SCRIPT_DIR/two_audio_tracks.mp4"

"$FFMPEG" -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=64x48:rate=10:duration=0.30" \
  -c:v mpeg4 -q:v 5 -an "${common_output[@]}" \
  "$SCRIPT_DIR/unsupported_codec.mp4"

(
  cd "$SCRIPT_DIR"
  shasum -a 256 ./*.mp4 | sed 's#  \./#  #'
)
