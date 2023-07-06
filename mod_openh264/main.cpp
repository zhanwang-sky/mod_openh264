//
//  main.cpp
//  mod_openh264
//
//  Created by Ji Chen on 2023/4/29.
//

#include <fstream>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "jpeg_helper.hpp"
#include "mod_openh264.hpp"
#include "ppm_helper.hpp"
#include "yuv_helper.hpp"

// Definitions

class NaluReader {
public:
  NaluReader(const char* filename) : fd_(open(filename, O_RDONLY)) { }

  virtual ~NaluReader() {
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
  }

  bool operator!() { return fd_ < 0; }

  int read(unsigned char* buf, int buf_sz) {
    static const std::string start_code = {'\00', '\00', '\00', '\01'};
    ssize_t nbytes = pread(fd_, buf, buf_sz, offst_);
    if (nbytes <= 0) {
      return (int) nbytes;
    }
    const std::string raw_data((const char*) buf, nbytes);
    std::string::size_type pos = raw_data.find(start_code, 1);
    if (pos == std::string::npos) {
      offst_ += nbytes;
      return (int) nbytes;
    }
    offst_ += pos;
    return (int) pos;
  }

private:
  int fd_ = -1;
  long offst_ = 0;
};

// Global Variables

DEFINE_string(input, "", "input.h264, mandatory");
DEFINE_string(output, "", "output.h264, optional");
DEFINE_string(pic, "", "pic_%03d.ppm, optional");

unsigned char yuv_buf[3840 * 2160 * 3];
unsigned char rgb_buf[3840 * 2160 * 3];
unsigned char nalu_buf[64 * UINT16_MAX];

// Global Functions

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  google::InitGoogleLogging(argv[0]);

  if (FLAGS_input.empty()) {
    LOG(ERROR) << "Usage: ./mod_openh264 <input.h264> [output.h264] [pic_%03d.ppm]";
    exit(EXIT_FAILURE);
  }

  NaluReader nalu_reader(FLAGS_input.c_str());
  if (!nalu_reader) {
    LOG(ERROR) << "Fail to open \"" << FLAGS_input << "\"";
    exit(EXIT_FAILURE);
  }

  std::ofstream nalu_dumper;
  if (!FLAGS_output.empty()) {
    nalu_dumper.open(FLAGS_output.c_str(), std::ofstream::binary | std::ofstream::trunc);
    if (!nalu_dumper) {
      LOG(ERROR) << "Fail to open \"" << FLAGS_output << "\"";
      exit(EXIT_FAILURE);
    }
  }

  auto h264_context = mod_openh264::switch_h264_init(true, true, 1920, 1080,
                                                     60.f, 16 * 1024 * 1024);
  if (!h264_context) {
    LOG(ERROR) << "Fail to init openh264";
    exit(EXIT_FAILURE);
  }

  int nalu_sz, rc;
  int nr_slices = 0;
  int nr_frames = 0;
  int nr_errors = 0;
  mod_openh264::switch_image img = {0};

  while ((nalu_sz = nalu_reader.read(nalu_buf, sizeof(nalu_buf))) > 0) {
    ++nr_slices;
    LOG(INFO) << "Slice " << nr_slices << ": " << nalu_sz << " bytes";

    rc = mod_openh264::switch_h264_decode(h264_context, nalu_buf, nalu_sz, &img);
    if (rc < 0) {
      ++nr_errors;
      LOG(ERROR) << "Decode error, slice " << nr_slices << ": rc=" << rc;
      continue;
    } else if (rc > 0) {
      // more data required
      continue;
    }

    ++nr_frames;
    LOG(INFO) << "Frame " << nr_frames << ": " << img.width << "*" << img.height;

    if (!FLAGS_pic.empty()) {
      char filename[256] = {0};
      snprintf(filename, sizeof(filename), FLAGS_pic.c_str(), nr_frames);
      yuv_helper::PlanarYUV_to_RGB(yuv_helper::CS_BT709, yuv_helper::FMT_I420,
                                   img.width, img.height,
                                   (const int*) img.stride,
                                   (const unsigned char**) img.planes,
                                   rgb_buf);
      ppm_helper::write_ppm(filename, img.width, img.height, rgb_buf);
    }

    if (nalu_dumper) {
      while ((nalu_sz = mod_openh264::switch_h264_encode(h264_context, &img, nalu_buf, sizeof(nalu_buf))) > 0) {
        nalu_dumper.write((const char*) nalu_buf, nalu_sz);
      }
    }
  }

  LOG(INFO) << nr_slices << " slices, " << nr_frames << " frames, " << nr_errors << " errors";

  return 0;
}
