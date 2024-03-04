#include "hdmi_dev.h"
#include "hdmi_fb.h"
#include "video.h"

#include <stdio.h>
#include <unistd.h>

//! \brief Print the usage and exit
//! \details Exits with code 1
__attribute__((noreturn)) void usage(void) {
  const char *const USAGE =
      "Usage: hdmi-dev-video-player [VIDEO]\n"
      "Plays the video file specified by [VIDEO] using the HDMI Peripheral\n"
      "\n"
      "The input video must be 640x480@15fps, and it must have frames encoded\n"
      "as YUV420P. It also cannot have any audio associated with it - it must\n"
      "be a single stream.\n"
      "\n"
      "Furthermore, this program must be used with the HDMI Peripheral. It\n"
      "must be run as root to interact with the device.\n";
  fputs(USAGE, stderr);
  exit(1);
}

//! \brief Stop the device and exit
//!
//! This method does not wait for the device to signal idle. It also bypasses
//! all the atexit hooks. It is intended to installed with `sigaction`.
__attribute__((noreturn)) void signalhandler(int signum) {
  // We don't care what signal we received, nor do we care about cleaning up our
  // resources.
  (void)signum;
  // Just stop the device and die.
  hdmi_dev_stopnow();
  _exit(2);
}

int main(int argc, char **argv) {

  // Check usage
  if (argc != 2) {
    fputs("Usage: wrong number of arguments\n", stderr);
    usage();
  } else if (geteuid() != 0) {
    fputs("Usage: must be run as root\n", stderr);
    usage();
  }

  // Open the video to play
  video_t *vid = video_open(argv[1]);
  if (vid == NULL) {
    fputs("Usage: failed to open video\n", stderr);
    usage();
  }

  // Create the framebuffer allocator ...
  hdmi_fb_allocator_t *alloc_fb = hdmi_fb_allocator_open();
  if (alloc_fb == NULL) {
    fputs("Error: failed to open framebuffer allocator\n", stderr);
    exit(127);
  }
  // ... so we can allocate two framebuffers to double-buffer with
  hdmi_fb_handle_t *fbs[2];
  for (size_t i = 0u; i < 2u; i++) {
    fbs[i] = hdmi_fb_allocate(alloc_fb);
    if (fbs[i] == NULL) {
      fputs("Error: failed to allocate framebuffer\n", stderr);
      exit(127);
    }
  }

  // Setup the device
  if (!hdmi_dev_open()) {
    fputs("Error: failed to open HDMI Peripheral\n", stderr);
    exit(127);
  }

  puts("TRACE: Done with setup!");

  // Keep reading frames until we hit the end of the file
  size_t fb = 0u;
  hdmi_coordinate_t last;
  bool first = true;
  while (true) {

    // Decode a frame into the current framebuffer. This will switch because
    // we're double buffering.
    int res = video_get_frame(vid, hdmi_fb_data(fbs[fb]));
    if (res == AVERROR_EOF) {
      fputs("TRACE: Hit EOF on video\n", stderr);
      break;
    } else if (res != 0) {
      fprintf(stderr, "Error: got %d when decoding video\n", res);
    }

    // Wait until 4 frames have elapsed since the last time we presented. If
    // we're on the first iteration, skip this step since we don't have a last
    // time.
    if (!first) {
      hdmi_coordinate_t cur = hdmi_dev_coordinate();
      while (hdmi_fid_delta(cur.fid, last.fid) < 4)
        cur = hdmi_dev_coordinate();
      last = cur;
    }

    // Present the new frame. If this is the first iteration, the device hasn't
    // started yet.
    hdmi_dev_set_fb(fbs[fb]);
    if (first)
      hdmi_dev_start();

    // If we're on the first iteration, remember to set the last coordinates. We
    // didn't do it when we were waiting above since the device hadn't started
    // yet.
    if (first)
      last = hdmi_dev_coordinate();

    // Next
    fb = (fb + 1u) & 1u;
    first = false;
  }

  // At least cleanup on the happy path
  puts("TRACE: Cleaning up...");
  hdmi_dev_stop();
  hdmi_dev_close();
  hdmi_fb_free(alloc_fb, fbs[0u]);
  hdmi_fb_free(alloc_fb, fbs[1u]);
  hdmi_fb_allocator_close(alloc_fb);
  video_close(vid);
  puts("TRACE: Cleaned up!");
  return 0;
}
