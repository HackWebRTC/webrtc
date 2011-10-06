/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <dirent.h>  // for checking directory existence

#include <cassert>
#include <cstdio>

#include "google/gflags.h"
#include "packet_manipulator.h"
#include "packet_reader.h"
#include "stats.h"
#include "trace.h"
#include "util.h"
#include "video_metrics.h"
#include "videoprocessor.h"
#include "vp8.h"

DEFINE_string(test_name, "Quality test", "The name of the test to run. "
              "Default: Quality test.");
DEFINE_string(test_description, "", "A more detailed description about what "
              "the current test is about.");
DEFINE_string(input_filename, "", "Input file. "
              "The source video file to be encoded and decoded. Must be in "
              ".yuv format");
DEFINE_int32(width, -1, "Width in pixels of the frames in the input file.");
DEFINE_int32(height, -1, "Height in pixels of the frames in the input file.");
DEFINE_int32(framerate, 30, "Frame rate of the input file, in FPS "
             "(frames-per-second). ");
DEFINE_string(output_dir, ".", "Output directory. "
              "The directory where the output file will be put. Must already "
              "exist.");
DEFINE_bool(use_single_core, false, "Force using a single core. If set to "
            "true, only one core will be used for processing. Using a single "
            "core is necessary to get a deterministic behavior for the"
            "encoded frames - using multiple cores will produce different "
            "encoded frames since multiple cores are competing to consume the "
            "byte budget for each frame in parallel. If set to false, "
            "the maximum detected number of cores will be used. ");
DEFINE_bool(disable_fixed_random_seed , false, "Set this flag to disable the"
            "usage of a fixed random seed for the random generator used "
            "for packet loss. Disabling this will cause consecutive runs "
            "loose packets at different locations, which is bad for "
            "reproducibility.");
DEFINE_string(output_filename, "", "Output file. "
              "The name of the output video file resulting of the processing "
              "of the source file. By default this is the same name as the "
              "input file with '_out' appended before the extension.");
DEFINE_int32(bitrate, 500, "Bit rate in kilobits/second.");
DEFINE_int32(packet_size, 1500, "Simulated network packet size in bytes (MTU). "
             "Used for packet loss simulation.");
DEFINE_int32(max_payload_size, 1440, "Max payload size in bytes for the "
             "encoder.");
DEFINE_string(packet_loss_mode, "uniform", "Packet loss mode. Two different "
              "packet loss models are supported: uniform or burst. This "
              "setting has no effect unless packet_loss_rate is >0. ");
DEFINE_double(packet_loss_probability, 0.0, "Packet loss probability. A value "
              "between 0.0 and 1.0 that defines the probability of a packet "
              "being lost. 0.1 means 10% and so on.");
DEFINE_int32(packet_loss_burst_length, 1, "Packet loss burst length. Defines "
             "how many packets will be lost in a burst when a packet has been "
             "decided to be lost. Must be >=1.");
DEFINE_bool(csv, false, "CSV output. Enabling this will output all frame "
            "statistics at the end of execution. Recommended to run combined "
            "with --noverbose to avoid mixing output.");

// Runs a quality measurement on the input file supplied to the program.
// The input file must be in YUV format.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage = "Quality test application for video comparisons.\n"
    "Run " + program_name + " --helpshort for usage.\n"
    "Example usage:\n" + program_name +
    " --input_filename=filename.yuv --width=352 --height=288\n";
  google::SetUsageMessage(usage);

  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_input_filename == "" || FLAGS_width == -1 || FLAGS_height == -1) {
    printf("%s\n", google::ProgramUsage());
    return 1;
  } else {
    webrtc::test::TestConfig config;
    config.name = FLAGS_test_name;
    config.description = FLAGS_test_description;

    // Verify the input file exists and is readable:
    FILE* test_file;
    test_file = fopen(FLAGS_input_filename.c_str(), "rb");
    if (test_file == NULL) {
      fprintf(stderr, "Cannot read the specified input file: %s\n",
              FLAGS_input_filename.c_str());
      return 2;
    }
    fclose(test_file);
    config.input_filename = FLAGS_input_filename;

    // Verify the output dir exists:
    DIR* output_dir = opendir(FLAGS_output_dir.c_str());
    if (output_dir == NULL) {
      fprintf(stderr, "Cannot find output directory: %s\n",
              FLAGS_output_dir.c_str());
      return 3;
    }
    closedir(output_dir);
    config.output_dir = FLAGS_output_dir;

    // Manufacture an output filename if none was given:
    if (FLAGS_output_filename == "") {
      // Cut out the filename without extension from the given input file
      // (which may include a path)
      int startIndex = FLAGS_input_filename.find_last_of("/") + 1;
      if (startIndex == 0) {
        startIndex = 0;
      }
      FLAGS_output_filename =
          FLAGS_input_filename.substr(startIndex,
                                      FLAGS_input_filename.find_last_of(".")
                                      - startIndex) + "_out.yuv";
    }

    // Verify output file can be written
    std::string output_filename;
    if (FLAGS_output_dir == ".") {
      output_filename = FLAGS_output_filename;
    } else {
      output_filename = FLAGS_output_dir + "/"+ FLAGS_output_filename;
    }
    test_file = fopen(output_filename.c_str(), "wb");
    if (test_file == NULL) {
      fprintf(stderr, "Cannot write output file: %s\n",
              output_filename.c_str());
      return 4;
    }
    fclose(test_file);
    config.output_filename = output_filename;

    // Check single core flag
    config.use_single_core = FLAGS_use_single_core;

    // Seed our random function if that flag is enabled. This will force
    // repeatable behaviour between runs
    if (!FLAGS_disable_fixed_random_seed) {
      srand(0);
    }

    // Check the bit rate
    if (FLAGS_bitrate <= 0) {
      fprintf(stderr, "Bit rate must be >0 kbps, was: %d\n", FLAGS_bitrate);
      return 5;
    }
    config.codec_settings.startBitrate = FLAGS_bitrate;

    // Check packet size and max payload size
    if (FLAGS_packet_size <= 0) {
      fprintf(stderr, "Packet size must be >0 bytes, was: %d\n",
              FLAGS_packet_size);
      return 6;
    }
    config.networking_config.packet_size_in_bytes = FLAGS_packet_size;

    if (FLAGS_max_payload_size <= 0) {
      fprintf(stderr, "Max payload size must be >0 bytes, was: %d\n",
              FLAGS_max_payload_size);
      return 7;
    }
    config.networking_config.max_payload_size_in_bytes = FLAGS_max_payload_size;

    // Check the width and height
    if (FLAGS_width <= 0 || FLAGS_height <= 0) {
      fprintf(stderr, "Width and height must be >0.");
      return 8;
    }
    config.codec_settings.width = FLAGS_width;
    config.codec_settings.height = FLAGS_height;

    // Check framerate
    if (FLAGS_framerate <= 0) {
      fprintf(stderr, "Framerate be >0.");
      return 9;
    }
    config.codec_settings.maxFramerate = FLAGS_framerate;

    // Check packet loss settings
    if (FLAGS_packet_loss_mode != "uniform" &&
        FLAGS_packet_loss_mode != "burst") {
      fprintf(stderr, "Unsupported packet loss mode, must be 'uniform' or "
              "'burst'\n.");
      return 10;
    }
    config.networking_config.packet_loss_mode = webrtc::test::kUniform;
    if (FLAGS_packet_loss_mode == "burst") {
      config.networking_config.packet_loss_mode =  webrtc::test::kBurst;
    }

    if (FLAGS_packet_loss_probability < 0.0 ||
        FLAGS_packet_loss_probability > 1.0) {
      fprintf(stderr, "Invalid packet loss probability. Must be 0.0 - 1.0, "
              "was: %f\n", FLAGS_packet_loss_probability);
      return 11;
    }
    config.networking_config.packet_loss_probability =
        FLAGS_packet_loss_probability;

    if (FLAGS_packet_loss_burst_length < 1) {
      fprintf(stderr, "Invalid packet loss burst length, must be >=1, "
              "was: %d\n", FLAGS_packet_loss_burst_length);
      return 12;
    }
    config.networking_config.packet_loss_burst_length =
        FLAGS_packet_loss_burst_length;

    // Calculate the size of each frame to read (according to YUV spec):
    int frame_length_in_bytes =
          3 * config.codec_settings.width * config.codec_settings.height / 2;

    log("Quality test with parameters:\n");
    log("  Test name        : %s\n", FLAGS_test_name.c_str());
    log("  Description      : %s\n", FLAGS_test_description.c_str());
    log("  Input filename   : %s\n", FLAGS_input_filename.c_str());
    log("  Output directory : %s\n", config.output_dir.c_str());
    log("  Output filename  : %s\n", output_filename.c_str());
    log("  Frame size       : %d bytes\n", frame_length_in_bytes);
    log("  Packet size      : %d bytes\n", FLAGS_packet_size);
    log("  Max payload size : %d bytes\n", FLAGS_max_payload_size);
    log("  Packet loss:\n");
    log("    Mode           : %s\n", FLAGS_packet_loss_mode.c_str());
    log("    Probability    : %2.1f\n", FLAGS_packet_loss_probability);
    log("    Burst length   : %d packets\n", FLAGS_packet_loss_burst_length);

    webrtc::VP8Encoder encoder;
    webrtc::VP8Decoder decoder;
    webrtc::test::Stats stats;
    webrtc::test::FileHandlerImpl file_handler(config.input_filename,
                                               config.output_filename,
                                               frame_length_in_bytes);
    file_handler.Init();
    webrtc::test::PacketReader packet_reader;

    webrtc::test::PacketManipulatorImpl packet_manipulator(
        &packet_reader, config.networking_config);
    webrtc::test::VideoProcessorImpl processor(&encoder, &decoder,
                                               &file_handler,
                                               &packet_manipulator,
                                               config, &stats);
    processor.Init();

    int frame_number = 0;
    while (processor.ProcessFrame(frame_number)) {
      if (frame_number % 80 == 0) {
        log("\n");  // make the output a bit nicer.
      }
      log(".");
      frame_number++;
    }
    log("\n");
    log("Processed %d frames\n", frame_number);

    // Release encoder and decoder to make sure they have finished processing:
    encoder.Release();
    decoder.Release();

    // Verify statistics are correct:
    assert(frame_number == static_cast<int>(stats.stats_.size()));

    // Close the files before we start using them for SSIM/PSNR calculations.
    file_handler.Close();

    stats.PrintSummary();

    // Calculate SSIM
    QualityMetricsResult ssimResult;
    log("Calculating SSIM...\n");
    SsimFromFiles(FLAGS_input_filename.c_str(), output_filename.c_str(),
                  config.codec_settings.width,
                  config.codec_settings.height, &ssimResult);
    log("  Average: %3.2f\n", ssimResult.average);
    log("  Min    : %3.2f (frame %d)\n", ssimResult.min,
        ssimResult.min_frame_number);
    log("  Max    : %3.2f (frame %d)\n", ssimResult.max,
        ssimResult.max_frame_number);

    QualityMetricsResult psnrResult;
    log("Calculating PSNR...\n");
    PsnrFromFiles(FLAGS_input_filename.c_str(), output_filename.c_str(),
                      config.codec_settings.width,
                      config.codec_settings.height, &psnrResult);
    log("  Average: %3.2f\n", psnrResult.average);
    log("  Min    : %3.2f (frame %d)\n", psnrResult.min,
        psnrResult.min_frame_number);
    log("  Max    : %3.2f (frame %d)\n", psnrResult.max,
        psnrResult.max_frame_number);

    if (FLAGS_csv) {
      log("\nCSV output (recommended to run with --noverbose to skip the "
          "above output)\n");
      printf("frame_number encoding_successful decoding_successful "
          "encode_return_code decode_return_code "
          "encode_time_in_us decode_time_in_us "
          "bit_rate_in_kbps encoded_frame_length_in_bytes frame_type "
          "packets_dropped total_packets "
          "ssim psnr\n");

      for (unsigned int i = 0; i < stats.stats_.size(); ++i) {
        webrtc::test::FrameStatistic& f = stats.stats_[i];
        FrameResult& ssim = ssimResult.frames[i];
        FrameResult& psnr = psnrResult.frames[i];
        printf("%4d, %d, %d, %2d, %2d, %6d, %6d, %5d, %7d, %d, %2d, %2d, "
               "%5.3f, %5.2f\n",
               f.frame_number,
               f.encoding_successful,
               f.decoding_successful,
               f.encode_return_code,
               f.decode_return_code,
               f.encode_time_in_us,
               f.decode_time_in_us,
               f.bit_rate_in_kbps,
               f.encoded_frame_length_in_bytes,
               f.frame_type,
               f.packets_dropped,
               f.total_packets,
               ssim.value,
               psnr.value);
      }
    }
    log("Quality test finished!");
    return 0;
  }
}


