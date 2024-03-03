#include "fb.h"
#include "hdmi_dev.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {

  // Check we are running as root. We need this to be able to flash the PL and
  // the deal with physical memory.
  if (geteuid() != 0) {
    fprintf(stderr, "Error: must be run as root\n");
    exit(1);
  }

  // Create and initialize the framebuffer allocator, checking for errors
  fb_allocator_t fb_allocator;
  if (!fb_allocator_init(&fb_allocator)) {
    fprintf(stderr, "Error: failed to open DRM device");
    exit(127);
  }

  // Initialize the device, checking for errors
  if (!hdmi_dev_open()) {
    fprintf(stderr, "Error: failed to initialize HDMI Peripheral\n");
    exit(127);
  }

  // Create a framebuffer and populate it with all red
  fb_t fb_red;
  if (!fb_allocate(fb_allocator, &fb_red)) {
    fprintf(stderr, "Error: failed to allocate framebuffer");
    exit(127);
  }
  for (size_t i = 0u; i < 640u * 480u; i++) {
    fb_ptr(fb_red)[i] = 0x00ff0000u;
  }

  hdmi_dev_start();

  size_t count = 1u;
  hdmi_coordinate_t last = hdmi_dev_coordinate();
  while (count <= 30u) {
    hdmi_coordinate_t cur = hdmi_dev_coordinate();
    int d = hdmi_fid_delta(last.fid, cur.fid);
    if (d <= -60) {
      printf("%u\n", count);
      last = cur;
      count++;
    }
  }

  // We're done, so free all the resources
  fb_free(fb_allocator, fb_red);
  fb_allocator_close(fb_allocator);
  hdmi_dev_close();
}
