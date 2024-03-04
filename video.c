#include "video.h"

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <stdlib.h>

video_t *video_open(const char *filename) {

  // Allocate space for the return value
  video_t *ret = calloc(1u, sizeof(video_t));
  if (ret == NULL)
    return NULL;
  // Initialize everything to a known state
  ret->format_ctx = NULL;
  ret->codec_ctx = NULL;
  ret->packet = NULL;
  ret->frame = NULL;

  // Open the input file, failing if we can't. This will allocate the context
  // for the container on success, placing the result in `ret->format_ctx`.
  if (avformat_open_input(&ret->format_ctx, filename, NULL, NULL) != 0)
    goto failure;

  // We should only have one stream. If we get a container with more than one,
  // just fail so we don't have to find the right one.
  if (ret->format_ctx->nb_streams != 1u)
    goto failure;

  // Pull out the stream for easy access. Note that this aliases into the format
  // context. It will be freed when the format context is freed.
  AVStream *stream = ret->format_ctx->streams[0u];

  // Validate that the stream is what we expect
  const AVCodecParameters *stream_codecpar = stream->codecpar;
  // The singular stream should be a video stream, ...
  if (stream_codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
    goto failure;
  // ... which is 640x480.
  if (stream_codecpar->width != 640 || stream_codecpar->height != 480)
    goto failure;
  // We can't validate the framerate since it might be unknown.

  // Find the codec we're supposed to use, and create the context for it based
  // on the parameters requested by the video.
  const AVCodec *codec = avcodec_find_decoder(stream_codecpar->codec_id);
  if (codec == NULL)
    goto failure;
  ret->codec_ctx = avcodec_alloc_context3(codec);
  if (ret->codec_ctx == NULL)
    goto failure;
  if (avcodec_parameters_to_context(ret->codec_ctx, stream_codecpar) < 0)
    goto failure;
  if (avcodec_open2(ret->codec_ctx, codec, NULL) != 0)
    goto failure;

  // Finally, allocate the packet and the frame we'll use for decoding
  ret->packet = av_packet_alloc();
  ret->frame = av_frame_alloc();
  if (ret->packet == NULL || ret->frame == NULL)
    goto failure;

  // We've setup everything we need to, so return
  return ret;

  // On failure, make sure to free all the resources we keep
failure:
  video_close(ret);
  return NULL;
}

void video_close(video_t *video) {
  // Edge case handling
  if (video == NULL)
    return;
  // Release all the resources. This is tolerant to having `NULL` values in
  // these fields. Also note that the format is guaranteed to either be `NULL`
  // or open because we allocated in `avformat_open_input`.
  av_packet_free(&video->packet);
  av_frame_free(&video->frame);
  avcodec_free_context(&video->codec_ctx);
  avformat_close_input(&video->format_ctx);
  free(video);
}
