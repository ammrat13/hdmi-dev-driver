#include "hdmi_dev.h"

#include <stdio.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
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

//! \brief Physical address of the HDMI Peripheral's registers
static const off_t REGISTERS_PHYS = 0x40000000;
//! \brief Length of the mapping to the HDMI Peripheral's registers
static const size_t REGISTERS_LEN = 0x20u;

//! \brief Handle to an HDMI Peripheral
//!
//! There is a global variable containing this structure. It is intialized once
//! the peripheral is flashed onto the PL. It's used to hold data about the
//! device that needs to persist across calls, such as where the framebuffer is
//! mapped in our address space.
typedef struct hdmi_dev_handle_t {
  //! \brief Whether the device is in the initialized state
  bool initialized;

  //! \brief File descriptor corresponding to `/dev/mem`
  int mem_fd;
  //! \brief Where the peripheral's configuration registers are mapped
  //! \see REGISTERS_LEN
  uint32_t *registers;
  //! \brief Where the framebuffer is mapped
  uint32_t *framebuffer;

} hdmi_dev_handle_t;

//! \brief Handle to the singleton HDMI Peripheral
//! \details All the methods below implicitly operate on this device.
static hdmi_dev_handle_t hdmi_dev = {
    .initialized = false,
    .mem_fd = -1,
    .registers = NULL,
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
      goto failure;
    }
    // Program the bitstream. Both of these writes should only take one call to
    // complete since the data is so short and because these are device files.
    int flag_res = write(flag_fd, "0", 1);
    int prog_res = write(prog_fd, FIRMWARE_NAME, strlen(FIRMWARE_NAME));
    // Free the resources and check for failures
    close(flag_fd);
    close(prog_fd);
    if (flag_res != 1 || prog_res != strlen(FIRMWARE_NAME))
      goto failure;
  }

  // Try to open device memory. Any failure after this point will have to close
  // the handle.
  hdmi_dev.mem_fd = open("/dev/mem", O_RDWR | O_DSYNC);
  if (hdmi_dev.mem_fd == -1)
    goto failure;
  // Map the peripheral's configuration registers. Again, any failure after this
  // point will have to unmap them.
  hdmi_dev.registers = mmap(NULL, REGISTERS_LEN, PROT_READ | PROT_WRITE,
                            MAP_SHARED, hdmi_dev.mem_fd, REGISTERS_PHYS);
  if (hdmi_dev.registers == MAP_FAILED)
    goto failure;

  // Configure the clocks
  {
    // Map the SLCR registers into our address space
    const off_t SLCR_PHYS = 0xf8000000;
    const size_t SLCR_LEN = 0x1000u;
    volatile uint32_t *slcr = mmap(NULL, SLCR_LEN, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, hdmi_dev.mem_fd, SLCR_PHYS);
    if (slcr == MAP_FAILED)
      goto failure;

    printf("IO_PLL_CTRL %08x\n", slcr[0x42u]);
    printf("PLL_STATUS %08x\n", slcr[0x43u]);

    // Cleanup. Note that this casts away volatile, but that's fine since this
    // will free the mapping.
    munmap((void *)slcr, SLCR_LEN);
  }

  // Mark as initialized and return
  hdmi_dev.initialized = true;
  return true;

  // On failure, reset our representation of the device to a known state, then
  // report the failure
failure:
  hdmi_dev_close();
  return false;
}

void hdmi_dev_close(void) {
  // This function is responsible for resetting the HDMI Peripheral to a known
  // state. It's used by the hdmi_dev_open function.

  // Close the registers which are mapped from device memory
  if (hdmi_dev.registers != NULL) {
    munmap(hdmi_dev.registers, REGISTERS_LEN);
    hdmi_dev.registers = NULL;
  }
  // Close the handle to devide memory
  if (hdmi_dev.mem_fd != -1) {
    close(hdmi_dev.mem_fd);
    hdmi_dev.mem_fd = -1;
  }

  // Mark as uninitialized
  hdmi_dev.initialized = false;
}
