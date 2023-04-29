//
//  mod_openh264.hpp
//  mod_openh264
//
//  Created by Ji Chen on 2023/4/29.
//

#ifndef mod_openh264_hpp
#define mod_openh264_hpp

#include <wels/codec_api.h>

namespace mod_openh264 {

enum h264_codec_cmd {
  CMD_GEN_KEYFRAME = 0,
  CMD_MAX,
};

struct h264_codec_context {
  ISVCEncoder* encoder;
  bool encoder_initialized;
  SEncParamExt encoder_params;
  SFrameBSInfo bit_stream_info;
  int cur_layer;
  int cur_nalu_index;
  int last_layer_pos;
  int last_nalu_pos;
  bool more_data;

  ISVCDecoder* decoder;
  bool decoder_initialized;
  SDecodingParam decoder_params;
};

struct switch_image {
  int width;
  int height;
  unsigned char* planes[4];
  int stride[4];
};

/**
 @param [IN] decoding create decoder
 @param [IN] encoding create encoder
 @param [IN] width encoding width
 @param [IN] height encoding height
 @param [IN] max_fps maximal input frame rate
 @param [IN] bitrate target bitrate desired, in unit of bps
 */
h264_codec_context* switch_h264_init(bool decoding, bool encoding,
                                     int width, int height,
                                     float max_fps, int bitrate);

/**
 @param [IN] context pointing to h264_codec_context
 */
void switch_h264_destroy(h264_codec_context* context);

/**
 @param [IN] context pointing to h264_codec_context
 @param [IN] nalu_buf encoded bitstream start position; should include start code prefix
 @param [IN] nalu_sz encoded bit stream length; should include the size of start code prefix
 @param [OUT] img output image, contains width, height, strides for each planes and YUV datas
 @retval negative - error, positive - more data required, 0 - done
 */
int switch_h264_decode(h264_codec_context* context,
                       const unsigned char* nalu_buf, int nalu_sz,
                       switch_image* img);

/**
 @param [IN] context pointing to h264_codec_context
 @param [IN] img input YUV image
 @param [OUT] buf bitstream data to write to
 @param [IN] buf_sz buffer size
 @retval negative - error, positive - bytes copied, 0 - done
 */
int switch_h264_encode(h264_codec_context* context,
                       const switch_image* img,
                       unsigned char* buf, int buf_sz);

/**
 @param [IN] context pointing to h264_codec_context
 @param [IN] cmd h264_codec_cmd
 @retval negative on error, 0 on success
 */
int switch_h264_control(h264_codec_context* context, h264_codec_cmd cmd);

}

#endif /* mod_openh264_hpp */
