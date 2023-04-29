//
//  mod_openh264.cpp
//  mod_openh264
//
//  Created by Ji Chen on 2023/4/29.
//

#include <iostream>
#include <memory>
#include "mod_openh264.hpp"

#define SLICE_SIZE 1200
#define NAL_HEADER_ADD_0X30BYTES 50

using namespace mod_openh264;
using std::cout;
using std::cerr;
using std::endl;

h264_codec_context* mod_openh264::switch_h264_init(bool decoding, bool encoding,
                                                   int width, int height,
                                                   float max_fps, int bitrate) {
  h264_codec_context* context = new(std::nothrow) h264_codec_context{0};
  if (!context) {
    cerr << "Fail to alloc h264_codec_context\n";
    goto err_exit;
  }

  if (decoding) {
    long rc = WelsCreateDecoder(&context->decoder);
    if (rc != 0) {
      cerr << "Fail to create Decoder, rc=" << rc << endl;
      goto err_exit;
    }

    context->decoder_params.uiTargetDqLayer = (uint8_t) -1;
    context->decoder_params.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    context->decoder_params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    context->decoder_params.sVideoProperty.size = sizeof(context->decoder_params.sVideoProperty);
    rc = context->decoder->Initialize(&context->decoder_params);
    if (rc != 0) {
      cerr << "Fail to initialize Decoder, rc=" << rc << endl;
      goto err_exit;
    }

    context->decoder_initialized = true;
  }

  if (encoding) {
    int rc = WelsCreateSVCEncoder(&context->encoder);
    if (rc != 0) {
      cerr << "Fail to create SVCEncoder, rc=" << rc << endl;
      goto err_exit;
    }

    SEncParamExt* param = &context->encoder_params;
    param->fMaxFrameRate = max_fps * 2;
    param->iPicWidth = width;
    param->iPicHeight = height;
    param->iTargetBitrate = bitrate;
    param->iRCMode = RC_QUALITY_MODE;
    param->iTemporalLayerNum = 1;
    param->iSpatialLayerNum = 1;
    param->bEnableDenoise = 0;
    param->bEnableBackgroundDetection = 1;
    param->bEnableSceneChangeDetect = 1;
    param->bEnableFrameSkip = 0;
    param->iMultipleThreadIdc = 1;
    param->bEnableAdaptiveQuant = 1;
    param->bEnableLongTermReference = 0;
    param->iLtrMarkPeriod = 30;
    param->iLoopFilterAlphaC0Offset = 0;
    param->iLoopFilterBetaOffset = 0;
    param->iComplexityMode = MEDIUM_COMPLEXITY;
    param->uiIntraPeriod = max_fps * 3;
    param->iNumRefFrame = AUTO_REF_PIC_COUNT;
    param->eSpsPpsIdStrategy = INCREASING_ID;
    param->bPrefixNalAddingCtrl = 0;

    int iIndexLayer = 0;
    param->sSpatialLayers[iIndexLayer].iVideoWidth = width;
    param->sSpatialLayers[iIndexLayer].iVideoHeight = height;
    param->sSpatialLayers[iIndexLayer].fFrameRate = max_fps;
    param->sSpatialLayers[iIndexLayer].iSpatialBitrate = param->iTargetBitrate;
    param->sSpatialLayers[iIndexLayer].iMaxSpatialBitrate = UNSPECIFIED_BIT_RATE;
    param->sSpatialLayers[iIndexLayer].uiLevelIdc = LEVEL_3_1;
    param->sSpatialLayers[iIndexLayer].uiProfileIdc = PRO_BASELINE;

    param->iUsageType = CAMERA_VIDEO_REAL_TIME;
    param->bEnableFrameCroppingFlag = 1;
    param->iMaxBitrate = UNSPECIFIED_BIT_RATE;

    param->sSpatialLayers[iIndexLayer].sSliceArgument.uiSliceMode = SM_SIZELIMITED_SLICE;
    param->sSpatialLayers[iIndexLayer].sSliceArgument.uiSliceSizeConstraint = SLICE_SIZE;
    param->uiMaxNalSize = SLICE_SIZE + NAL_HEADER_ADD_0X30BYTES;

    float fMaxFr = param->sSpatialLayers[param->iSpatialLayerNum - 1].fFrameRate;
    for (int i = param->iSpatialLayerNum - 2; i >= 0; --i) {
      if (param->sSpatialLayers[i].fFrameRate > fMaxFr + 0.000001f) {
        fMaxFr = param->sSpatialLayers[i].fFrameRate;
      }
    }
    param->fMaxFrameRate = fMaxFr;

    rc = context->encoder->InitializeExt(&context->encoder_params);
    if (rc != 0) {
      cerr << "Fail to initialize Encoder, rc=" << rc << endl;
      goto err_exit;
    }

    context->encoder_initialized = true;
  }

  return context;

err_exit:
  switch_h264_destroy(context);
  return NULL;
}

void mod_openh264::switch_h264_destroy(h264_codec_context* context) {
  if (context) {
    if (context->decoder) {
      if (context->decoder_initialized) {
        context->decoder->Uninitialize();
        context->decoder_initialized = false;
      }
      WelsDestroyDecoder(context->decoder);
      context->decoder = NULL;
    }
    if (context->encoder) {
      if (context->encoder_initialized) {
        context->encoder->Uninitialize();
        context->encoder_initialized = false;
      }
      WelsDestroySVCEncoder(context->encoder);
      context->encoder = NULL;
    }
    delete context;
  }
}

int mod_openh264::switch_h264_decode(h264_codec_context* context,
                                     const unsigned char* nalu_buf, int nalu_sz,
                                     switch_image* img) {
  // sanity check
  if (!context || !context->decoder || !context->decoder_initialized) {
    return INT_MIN;
  }

  unsigned char* pData[3] = {0};
  SBufferInfo dest_buffer_info = {0};

  auto decoder = context->decoder;
  auto rc = decoder->DecodeFrameNoDelay(nalu_buf, nalu_sz,
                                        pData, &dest_buffer_info);
  if (rc != dsErrorFree) {
    cerr << "Decode error, decoder=" << decoder << ", rc=" << rc << endl;
    return -1;
  }

  if (dest_buffer_info.iBufferStatus != 1) {
    // one frame data is not ready
    return 1;
  }

  img->width = dest_buffer_info.UsrData.sSystemBuffer.iWidth;
  img->height = dest_buffer_info.UsrData.sSystemBuffer.iHeight;
  img->planes[0] = pData[0];
  img->planes[1] = pData[1];
  img->planes[2] = pData[2];
  img->planes[3] = NULL;
  img->stride[0] = dest_buffer_info.UsrData.sSystemBuffer.iStride[0];
  img->stride[1] = dest_buffer_info.UsrData.sSystemBuffer.iStride[1];
  img->stride[2] = dest_buffer_info.UsrData.sSystemBuffer.iStride[1];
  img->stride[3] = 0;

  return 0;
}

int mod_openh264::switch_h264_encode(h264_codec_context* context,
                                     const switch_image* img,
                                     unsigned char* buf, int buf_sz) {
  // sanity check
  if (!context || !context->encoder || !context->encoder_initialized || buf_sz < 1) {
    return INT_MIN;
  }

  if (!context->more_data) {
    SSourcePicture pic = {0};
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = img->stride[0];
    pic.iStride[1] = img->stride[1];
    pic.iStride[2] = img->stride[2];
    pic.iStride[3] = img->stride[3];
    pic.pData[0] = img->planes[0];
    pic.pData[1] = img->planes[1];
    pic.pData[2] = img->planes[2];
    pic.pData[3] = img->planes[3];
    pic.iPicWidth = img->width;
    pic.iPicHeight = img->height;

    context->cur_layer = 0;
    context->cur_nalu_index = 0;
    context->last_layer_pos = 0;
    context->last_nalu_pos = 0;

    auto encoder = context->encoder;
    auto rc = encoder->EncodeFrame(&pic, &context->bit_stream_info);
    if (rc != 0) {
      cerr << "Encode error, encoder=" << encoder << ", rc=" << rc << endl;
      return -1;
    }
    context->more_data = true;
  }

  auto bit_stream_info = &context->bit_stream_info;
  if (context->last_nalu_pos >= bit_stream_info->sLayerInfo[context->cur_layer].pNalLengthInByte[context->cur_nalu_index]) {
    context->cur_nalu_index += 1;
    context->last_nalu_pos = 0;
  }

  if (context->cur_nalu_index >= bit_stream_info->sLayerInfo[context->cur_layer].iNalCount) {
    context->cur_layer += 1;
    context->cur_nalu_index = 0;
    context->last_layer_pos = 0;
    context->last_nalu_pos = 0;
  }

  if (context->cur_layer >= bit_stream_info->iLayerNum) {
    context->more_data = false;
    return 0;
  }

//  for (int i = 0; i < bit_stream_info->iLayerNum; ++i) {
//    for (int j = 0; j < bit_stream_info->sLayerInfo[i].iNalCount; ++j) {
//      int nalu_len = bit_stream_info->sLayerInfo[i].pNalLengthInByte[j];
//      cout << "[" << i << "][" << j << "]: " << nalu_len << endl;
//    }
//  }
//
//  return 0;

  int remain_nalu_sz = bit_stream_info->sLayerInfo[context->cur_layer].pNalLengthInByte[context->cur_nalu_index] - context->last_nalu_pos;
  int copy_sz = (remain_nalu_sz < buf_sz) ? remain_nalu_sz : buf_sz;
  memcpy(buf, bit_stream_info->sLayerInfo[context->cur_layer].pBsBuf + context->last_layer_pos, copy_sz);
  context->last_nalu_pos += copy_sz;
  context->last_layer_pos += copy_sz;

  return copy_sz;
}

int mod_openh264::switch_h264_control(h264_codec_context* context,
                                      h264_codec_cmd cmd) {
  int retval = 0;

  // sanity check
  if (!context) {
    return INT_MIN;
  }

  switch (cmd) {
    case CMD_GEN_KEYFRAME:
      // sanity check
      if (!context->encoder || !context->encoder_initialized) {
        retval = -2;
        break;
      }

      do {
        auto rc = context->encoder->ForceIntraFrame(1);
        if (rc != 0) {
          cerr << "Fail to generate key frame, rc=" << rc << endl;
          retval = -1;
        }
      } while (0);

      break;

    default:
      cerr << "Invalid cmd: " << cmd << endl;
      retval = INT_MIN + 1;
      break;
  }

  return retval;
}
