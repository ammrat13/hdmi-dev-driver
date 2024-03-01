#include "hdmi_dev.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

//! \brief Name of the firmware to program onto the PL
//!
//! This name corresponds to a `.bin` file in `/lib/firmware`. The `.bin` file
//! can be generates from a `.bit` file using the `bit2bin.py` script.
//!
//! \see
//! https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841847/Solution+ZynqMP+PL+Programming
static const char *const FIRMWARE_NAME = "hdmi_dev.bin";

//! \brief Sysfs files for programming the FPGA
//!
//! \see
//! https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841847/Solution+ZynqMP+PL+Programming
//!
//! @{
static const char *const FLAG_FILE = "/sys/class/fpga_manager/fpga0/flags";
static const char *const PROG_FILE = "/sys/class/fpga_manager/fpga0/firmware";
//! @}

//! \brief Handle to an HDMI Peripheral
//!
//! There is a global variable containing this structure. It is intialized once
//! the peripheral is flashed onto the PL. It's used to hold data about the
//! device that needs to persist across calls, such as where the framebuffer is
//! mapped in our address space.
typedef struct hdmi_dev_handle_t {
  //! \brief Whether the device is in the initialized state
  bool initialized;

  //! \brief Where the framebuffer is mapped in our address space
  uint8_t *framebuffer;

} hdmi_dev_handle_t;

//! \brief Handle to the singleton HDMI Peripheral
//! \details All the methods below implicitly operate on this device.
static hdmi_dev_handle_t hdmi_dev = {
    .initialized = false,
    .framebuffer = NULL,
};

bool hdmi_dev_open(void) {

  // If the device is already initialized, we don't have to do anything to get
  // it into an initialized state
  if (hdmi_dev.initialized)
    return true;

  // Program the PL
  {
    // Open the files
    int flag_fd = open(FLAG_FILE, O_WRONLY | O_DSYNC);
    int prog_fd = open(PROG_FILE, O_WRONLY | O_DSYNC);
    if (flag_fd == -1 || prog_fd == -1) {
      if (flag_fd != -1)
        close(flag_fd);
      if (prog_fd != -1)
        close(prog_fd);
      return false;
    }
    // Program the bitstream. Both of these writes should only take one call to
    // complete since the data is so short and because these are device files.
    int flag_res = write(flag_fd, "0", 1);
    int prog_res = write(prog_fd, FIRMWARE_NAME, strlen(FIRMWARE_NAME));
    // Free the resources and check for failures
    close(flag_fd);
    close(prog_fd);
    if (flag_res != 1 || prog_res != strlen(FIRMWARE_NAME))
      return false;
  }

  // Mark as initialized and return
  hdmi_dev.initialized = true;
  return true;
}

void hdmi_dev_close(void) {}
