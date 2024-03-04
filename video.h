#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct video_t {
  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVPacket *packet;
  AVFrame *frame;
} video_t;

video_t *video_open(const char *filename);
void video_close(video_t *video);
