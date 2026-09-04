# File capturer fixtures

These small MP4 files are deterministic M2 acceptance inputs. Regenerate them
with `./generate_fixtures.sh`; production builds and tests consume the checked-in
files and do not invoke a system FFmpeg binary.

The generation script records the codec, track, duration, time-offset and sample
rate choices directly in each command. `SHA256SUMS` pins the resulting bytes.

| Fixture | Contract covered |
| --- | --- |
| `audio_only.mp4` | AAC-only, 44.1 kHz mono conversion |
| `video_only.mp4` | H.264-only output |
| `audio_video.mp4` | ordinary dual-track capture and loop |
| `audio_video_b.mp4` | second dual-track source with distinct hue and 330 Hz tone |
| `audio_shorter.mp4` | audio-defined common boundary |
| `video_shorter.mp4` | video-defined common boundary |
| `nonzero_start.mp4` | edit-list/non-zero PTS normalization |
| `non_aligned.mp4` | loop duration flooring to 10 ms |
| `gap_over_2s.mp4` | a video gap beyond the lookahead horizon |
| `no_common_interval.mp4` | invalid disjoint track intervals |
| `two_audio_tracks.mp4` | explicit stream selection |
| `unsupported_yuv444.mp4` | unsupported decoded pixel format |
| `unsupported_codec.mp4` | unsupported MP4 video codec |
| `truncated_audio_video.mp4` | deterministic playback/decode failure |
