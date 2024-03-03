//! \file hdmi_dev.h
//! \brief Commands to interact with the HDMI Peripheral
//!
//! There is only one HDMI peripheral in existence - it is necessarily a
//! singleton. The methods here control that single peripheral.
//!
//! Note that these methods are not reentrant nor thread-safe.

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! \brief Initialize the HDMI Peripheral
//!
//! This method flashes the peripheral onto the PL. It also configures the
//! clocks and allocates memory for the framebuffer. If this method fails, the
//! PL and the clocks will be in an undefined state, but no resources will be
//! leaked.
//!
//! \return Whether initialization was successful
bool hdmi_dev_open(void);
//! \brief Inverse of `hdmi_dev_open`
//!
//! This method frees all the host-side resources currently in use by the
//! peripheral. However, it leaves the PL and the clocks in the same state.
void hdmi_dev_close(void);

//! \brief Set the HDMI Peripheral running
//!
//! This function will start running the device in continuous mode. So, it will
//! continuously be reading from the framebuffer and displaying that.
//!
//! This method should be called after the device is opened. It is a no-op if
//! it's not.
void hdmi_dev_start(void);
//! \brief Inverse of `hdmi_dev_start`
//!
//! Again, if this method is called when the HDMI Peripheral is not open, it is
//! a no-op.
void hdmi_dev_stop(void);

//! \brief Type for frame ids reported by the HDMI Peripheral
//!
//! These values are represented as `size_t`s, but they play by special rules.
//! They are always unsigned integers, but they don't span the full 32-bit
//! range. They are also relative - it only makes sense to ask for the delta
//! between two sufficiently close frame ids.
//!
//! \see hdmi_fid_delta
typedef uint_fast16_t hdmi_fid_t;

//! \brief Computes the difference between two frame ids
//!
//! These frame ids have to be "close enough". Currently, that means the two
//! frame ids differ by no more than 2048 frames.
//!
//! \return How many frames elapsed from `initial` to `final`
static inline int_fast16_t hdmi_fid_delta(hdmi_fid_t final,
                                          hdmi_fid_t initial) {
  // Subtract the two values modulo 2**12, then sign extend to 16-bit
  // See: https://stackoverflow.com/a/17719010
  int_fast16_t d = (final - initial) & 0xfffu;
  int_fast16_t m = 1u << 11;
  return (d ^ m) - m;
}

//! \brief Coordinates for pixels serialized by the HDMI Peripheral
//!
//! The device returns these coordinates to tell us where it is on the current
//! frame, and which frame it's on. The row and column are absolute, and they
//! range over [0, 525) and [0, 800) respectively. The frame id is relative. We
//! can subtract two frame ids to see how many frames have elapsed, but there is
//! no zero point.
typedef struct hdmi_coordinate_t {
  hdmi_fid_t fid;
  uint_fast16_t row;
  uint_fast16_t col;
} hdmi_coordinate_t;

//! \brief Get the current coordinate the HDMI Peripheral is serializing
//!
//! The result is only valid if the device has been started. Otherwise, this
//! method just returns garbage.
hdmi_coordinate_t hdmi_dev_coordinate(void);
