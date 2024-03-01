#include "hdmi_dev.h"

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

  printf("HERE\n");

  // We're done, so free all the resources
  hdmi_dev_close();
}
