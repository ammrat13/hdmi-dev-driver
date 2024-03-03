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

  // Initialize the device, checking for errors
  if (!hdmi_dev_open()) {
    fprintf(stderr, "Error: failed to initialize HDMI Peripheral\n");
    exit(127);
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
  hdmi_dev_close();
}
