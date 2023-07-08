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

DEFINE_string(in, "", "input H.264 stream");
DEFINE_string(out, "", "output H.264 stream");
DEFINE_string(pic, "", "picture sequence");
DEFINE_bool(p2v, false, "picture sequence to h264 video");

unsigned char yuv_buf[3840 * 2160 * 3];
unsigned char rgb_buf[3840 * 2160 * 3];
unsigned char nalu_buf[64 * UINT16_MAX];

// Global Functions

int pic_to_h264(const char* pic_in, const char* h264_out,
                mod_openh264::h264_codec_context* h264_codec) {
  char pic_name[256];
  int width, height;
  int nr_frames = 0;
  int rc = 0;
  mod_openh264::switch_image img = {0};

  std::ofstream nalu_dumper(h264_out, std::ofstream::binary | std::ofstream::trunc);
  if (!nalu_dumper) {
    LOG(ERROR) << "Fail to open \"" << h264_out << "\" for writing";
    return -1;
  }

  do {
    snprintf(pic_name, sizeof(pic_name), pic_in, nr_frames + 1);
    rc = ppm_helper::read_ppm(pic_name, &width, &height, rgb_buf, sizeof(rgb_buf));
    if (rc < 0) {
      LOG(ERROR) << "Fail to read ppm file \"" << pic_name << "\", rc=" << rc;
      break;
    }

    img.width = width;
    img.height = height;
    img.stride[0] = width;
    img.stride[1] = width;
    img.stride[2] = width;
    img.planes[0] = &yuv_buf[0];
    img.planes[1] = &yuv_buf[width * height];
    img.planes[2] = &yuv_buf[width * height * 2];

    yuv_helper::RGB_to_PlanarYUV(yuv_helper::CS_BT709, yuv_helper::FMT_I420, false,
                                 width, height, rgb_buf, img.stride, img.planes);

    while ((rc = mod_openh264::switch_h264_encode(h264_codec, &img, nalu_buf, sizeof(nalu_buf))) > 0) {
      nalu_dumper.write((const char*) nalu_buf, rc);
    }
    if (rc < 0) {
      LOG(ERROR) << "Encode error, frame " << nr_frames + 1 << ": rc=" << rc;
      break;
    }

    ++nr_frames;
  } while (1);

  LOG(INFO) << nr_frames << " frames encoded";

  return 0;
}

int h264_to_h264(const char* h264_in, const char* h264_out, const char* pic_out,
                 mod_openh264::h264_codec_context* h264_codec) {
  int nalu_sz, rc;
  int nr_slices = 0;
  int nr_frames = 0;
  int nr_errors = 0;
  mod_openh264::switch_image img = {0};

  NaluReader nalu_reader(h264_in);
  if (!nalu_reader) {
    LOG(ERROR) << "Fail to open \"" << h264_in << "\" for reading";
    return -1;
  }

  std::ofstream nalu_dumper;
  if (strlen(h264_out)) {
    nalu_dumper.open(h264_out, std::ofstream::binary | std::ofstream::trunc);
    if (!nalu_dumper) {
      LOG(ERROR) << "Fail to open \"" << h264_out << "\" for writing";
      return -1;
    }
  }

  while ((nalu_sz = nalu_reader.read(nalu_buf, sizeof(nalu_buf))) > 0) {
    ++nr_slices;
    LOG(INFO) << "Slice " << nr_slices << ": " << nalu_sz << " bytes";

    rc = mod_openh264::switch_h264_decode(h264_codec, nalu_buf, nalu_sz, &img);
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

    if (strlen(pic_out)) {
      char pic_name[256] = {0};
      snprintf(pic_name, sizeof(pic_name), pic_out, nr_frames);
      yuv_helper::PlanarYUV_to_RGB(yuv_helper::CS_BT709, yuv_helper::FMT_I420, false,
                                   img.width, img.height,
                                   (const int*) img.stride,
                                   (const unsigned char**) img.planes,
                                   rgb_buf);
      ppm_helper::write_ppm(pic_name, img.width, img.height, rgb_buf);
    }

    if (nalu_dumper) {
      while ((nalu_sz = mod_openh264::switch_h264_encode(h264_codec, &img, nalu_buf, sizeof(nalu_buf))) > 0) {
        nalu_dumper.write((const char*) nalu_buf, nalu_sz);
      }
    }
  }

  LOG(INFO) << nr_frames << " frames decoded, " << nr_slices << " slices, " << nr_errors << " errors";

  return 0;
}

int main(int argc, char* argv[]) {
  int ret = 0;
  FLAGS_logtostderr = true;
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  google::InitGoogleLogging(argv[0]);

  auto h264_codec = mod_openh264::switch_h264_init(true, true, 1920, 1080,
                                                   60.f, 16 * 1024 * 1024);
  if (!h264_codec) {
    LOG(ERROR) << "Fail to init openh264";
    exit(EXIT_FAILURE);
  }

  if (FLAGS_p2v) {
    ret = pic_to_h264(FLAGS_pic.c_str(), FLAGS_out.c_str(), h264_codec);
  } else {
    ret = h264_to_h264(FLAGS_in.c_str(), FLAGS_out.c_str(), FLAGS_pic.c_str(), h264_codec);
  }

  if (ret != 0) {
    exit(EXIT_FAILURE);
  }

  return 0;
}
