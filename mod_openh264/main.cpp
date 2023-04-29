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
#include "mod_openh264.hpp"

using std::cout;
using std::cerr;
using std::endl;

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

unsigned char yuv_buf[3840 * 2160 * 3];
unsigned char nalu_buf[64 * UINT16_MAX];

// Global Functions

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./mod_openh264 <input.h264> <output.h264>\n";
    exit(EXIT_FAILURE);
  }

  NaluReader nalu_reader(argv[1]);
  if (!nalu_reader) {
    cerr << "Fail to open \"" << argv[1] << "\" for reading\n";
    exit(EXIT_FAILURE);
  }

  std::ofstream nalu_dumper(argv[2], std::ofstream::binary | std::ofstream::trunc);
  if (!nalu_dumper) {
    cerr << "Fail to open \"" << argv[2] << "\" for writing\n";
    exit(EXIT_FAILURE);
  }

  auto h264_context = mod_openh264::switch_h264_init(true, true, 1920, 1080,
                                                     60.f, 16 * 1024 * 1024);
  if (!h264_context) {
    cerr << "Fail to init mod_openh264\n";
    exit(EXIT_FAILURE);
  }

  mod_openh264::switch_image img = {0};
  int nalu_sz, rc;
  int nr_slices = 0;
  int nr_frames = 0;
  while ((nalu_sz = nalu_reader.read(nalu_buf, sizeof(nalu_buf))) > 0) {
    ++nr_slices;
    cout << "Slice " << nr_slices << ": " << nalu_sz << endl;
    rc = mod_openh264::switch_h264_decode(h264_context, nalu_buf, nalu_sz, &img);
    if (rc < 0) {
      cerr << "Decode error, slice=" << nr_slices << ", rc=" << rc << endl;
      continue;
    }
    if (rc == 0) {
      ++nr_frames;
      cout << "Frame " << nr_frames << ": " << img.width << "*" << img.height << endl;
      while ((nalu_sz = mod_openh264::switch_h264_encode(h264_context, &img, nalu_buf, sizeof(nalu_buf))) > 0) {
        nalu_dumper.write((const char*) nalu_buf, nalu_sz);
      }
    }
  }

  cout << nr_slices << " slices, " << nr_frames << " frames\n";

  return 0;
}
