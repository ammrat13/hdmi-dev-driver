//! \file video.h
//! \brief Functions to read video data from files
//!
//! This module only interacts with very particular videos. The videos cannot
//! have any audio associated with them. They also have to be 640x480 and the
//! pixel format has to be YUV420P.

#pragma once

#include <stdint.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

//! \brief Persistent data we need to decode videos
//!
//! This structure holds the context LibAV needs for decoding. It also holds the
//! packet and frame we use for decoding. We allocate these once at startup and
//! reuse them.
//!
//! The `packet` and `frame` fields should normally hold no data. They are
//! allocated when reading a frame and unreferenced after that.
typedef struct video_t {
  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  AVPacket *packet;
  AVFrame *frame;
} video_t;

//! \brief Open a video file
//!
//! As mentioned above, we only handle very particular files. The videos have to
//! have just one stream, they must be 640x480, and they must have a pixel
//! format of YUV420P. We can't validate the the pixel format here, so you might
//! have `video_get_frame` fail if that constraint is violated. We also can't
//! find the frame rate.
//!
//! \param[in] filename The file we should try to open as a video
//! \return A handle to the video, or `NULL` on failure
video_t *video_open(const char *filename);
//! \brief Inverse of `video_open`
//! \details Video handles must be freed to prevent resource leaks
void video_close(video_t *video);

//! \brief Read one frame from the video
//!
//! If one of the arguments is `NULL`, or if the video data is not YUV420P, this
//! function returns `AVERROR(EINVAL)`. Otherwise, this function forwards the
//! error returned by LibAV. Importantly, this means that `AVERROR_EOF` is
//! returned on end-of-file.
//!
//! \param[in] video The video to read a frame from
//! \param[out] framebuffer Where to write the pixel data for the frame
//! \return Zero on success, or an error
int video_get_frame(video_t *video, uint32_t *framebuffer);
