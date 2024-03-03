#include "hdmi_dev.h"

#include <fcntl.h>
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
  //! \details The default value must be `-1`
  int mem_fd;
  //! \brief Where the peripheral's configuration registers are mapped
  //! \details The default value must be `MAP_FAILED`
  //! \see REGISTERS_LEN
  volatile uint32_t *registers;

} hdmi_dev_handle_t;

//! \brief Handle to the singleton HDMI Peripheral
//! \details All the methods below implicitly operate on this device.
static hdmi_dev_handle_t hdmi_dev = {
    .initialized = false,
    .mem_fd = -1,
    .registers = MAP_FAILED,
};

//! \brief Initialize the PL with the HDMI Peripheral
//! \see hdmi_dev_open
static bool init_pl(void) {

  // Open flag and programming files. Make sure to check for errors.
  int flag_fd = open(FLAG_FILE, O_WRONLY | O_DSYNC);
  int prog_fd = open(PROG_FILE, O_WRONLY | O_DSYNC);
  if (flag_fd == -1 || prog_fd == -1)
    goto failure;

  // Program the bitstream. Both of these writes should only take one call to
  // complete since the data is so short and because these are device files.
  int flag_res = write(flag_fd, "0", 1);
  if (flag_res != 1)
    goto failure;
  int prog_res = write(prog_fd, FIRMWARE_NAME, strlen(FIRMWARE_NAME));
  if (prog_res != strlen(FIRMWARE_NAME))
    goto failure;

  // We got here, so we've successfully written the bitstream. Close all our
  // open file descriptors and return success.
  close(flag_fd);
  close(prog_fd);
  return true;

  // On failure, close the files we have open. We don't write them into the
  // `hdmi_dev` structure, so we have to do it ourselves.
failure:
  if (flag_fd != -1)
    close(flag_fd);
  if (prog_fd != -1)
    close(prog_fd);
  return false;
}

//! \brief Get the file descriptor for `/dev/mem` and map the registers
//! \see hdmi_dev_open
static bool init_regs(void) {

  // If it looks like the registers have already been initialized, fail. We
  // should never be called from an initialized state.
  if (hdmi_dev.mem_fd != -1 || hdmi_dev.registers != MAP_FAILED)
    return false;

  // Try to open device memory
  hdmi_dev.mem_fd = open("/dev/mem", O_RDWR | O_DSYNC);
  if (hdmi_dev.mem_fd == -1)
    return false;
  // Now map the configuration registers. We're allowed to dump state into the
  // `hdmi_dev` variable, so wre don't have to close the file descriptor.
  hdmi_dev.registers = mmap(NULL, REGISTERS_LEN, PROT_READ | PROT_WRITE,
                            MAP_SHARED, hdmi_dev.mem_fd, REGISTERS_PHYS);
  if (hdmi_dev.registers == MAP_FAILED)
    return false;

  return true;
}

//! \brief Configure the clocks and reset the PL
//! \details This must be called after `init_pl` and `init_regs`
//! \see hdmi_dev_open
static bool init_clocks(void) {

  // Constants for the SLCR registers
  const off_t SLCR_PHYS = 0xf8000000;
  const size_t SLCR_LEN = 0x1000u;

  // If it looks like the registers haven't been initialized yet, fail
  if (hdmi_dev.mem_fd == -1)
    return false;

  // Map the SLCR registers into our address space. We're responsible for
  // cleaning this up.
  volatile uint32_t *slcr = mmap(NULL, SLCR_LEN, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, hdmi_dev.mem_fd, SLCR_PHYS);
  if (slcr == MAP_FAILED)
    return false;

  // The registers should be unlocked. If they're not, fail.
  if (slcr[0xcu / 4u] != 0u)
    goto failure;

  // Put the peripheral into reset. We only need Reset 0, but we'll do all of
  // them just in case.
  slcr[0x240u / 4u] = 0xfu;

  // Read the IO PLL divider value
  uint32_t iopll_div = (slcr[0x108u / 4u] >> 12) & 0x7f;
  // Set the clock divider for Clock 0. The input clock is 50MHz, so we want to
  // divide by half the `iopll_div` value to get a 100MHz clock.
  uint32_t clk_cfg = 0u;
  clk_cfg |= 0u << 4;              // Source from IO PLL
  clk_cfg |= (iopll_div / 2) << 8; // First divider is iopll_div / 2
  clk_cfg |= 1u << 20;             // Second divider is 1
  slcr[0x170u / 4u] = clk_cfg;

  // Pull the peripheral out of reset
  slcr[0x240u / 4u] = 0x0u;

  // Cleanup and succeed. Note that this casts away volatile, but that's fine
  // since this will free the mapping.
  munmap((void *)slcr, SLCR_LEN);
  return true;

  // On failure, do the same thing except return false
failure:
  munmap((void *)slcr, SLCR_LEN);
  return false;
}

bool hdmi_dev_open(void) {

  // If the device is already initialized, we don't have to do anything to get
  // it into an initialized state
  if (hdmi_dev.initialized)
    return true;

  // Perform all of the initialization tasks. These functions will clean up
  // their own resources, but they may leave some in the `hdmi_dev` variable for
  // us to clean up. That's why we call `hdmi_dev_close` on failure.
  if (!init_pl())
    goto failure;
  if (!init_regs())
    goto failure;
  if (!init_clocks())
    goto failure;

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
  // state. It's used by the `hdmi_dev_open` function.

  // If the device is running, stop it
  hdmi_dev_stop();

  // Close the registers which are mapped from device memory
  if (hdmi_dev.registers != MAP_FAILED) {
    munmap((void *)hdmi_dev.registers, REGISTERS_LEN);
    hdmi_dev.registers = MAP_FAILED;
  }
  // Close the handle to devide memory
  if (hdmi_dev.mem_fd != -1) {
    close(hdmi_dev.mem_fd);
    hdmi_dev.mem_fd = -1;
  }

  // Mark as uninitialized
  hdmi_dev.initialized = false;
}

void hdmi_dev_start(void) {
  // Check to make sure we have registers. If we don't, bail.
  if (hdmi_dev.registers == MAP_FAILED)
    return;
  // Otherwise, set it running in continuous mode. Wait until the current
  // coordinate goes valid to know that we're running. Remember to clear the
  // coordinate valid bit first.
  (void)hdmi_dev.registers[0x1cu / 4u];
  hdmi_dev.registers[0x0u / 4u] = 0x81u;
  while ((hdmi_dev.registers[0x1cu / 4u] & 1u) == 0u)
    ;
}

void hdmi_dev_stop(void) {
  // Check to make sure we have registers. If we don't, bail.
  if (hdmi_dev.registers == MAP_FAILED)
    return;
  // Otherwise, stop it. Wait until the device signals idle.
  hdmi_dev.registers[0x0u / 4u] = 0x00u;
  while ((hdmi_dev.registers[0x0u / 4u] & 0x04u) == 0u)
    ;
}

hdmi_coordinate_t hdmi_dev_coordinate(void) {
  // Populate the return value. It's just garbage if we need to bail.
  hdmi_coordinate_t ret = {
      .fid = 0u,
      .row = 0u,
      .col = 0u,
  };
  // If we don't have the registers mapped, bail
  if (hdmi_dev.registers == MAP_FAILED)
    return ret;
  // Otherwise, read the raw coordinate and populate the fields
  uint32_t raw_coord = hdmi_dev.registers[0x18u / 4u];
  ret.fid = (raw_coord >> 20) & 0xfffu;
  ret.row = (raw_coord >> 10) & 0x3ffu;
  ret.col = (raw_coord >> 0) & 0x3ffu;
  return ret;
}
