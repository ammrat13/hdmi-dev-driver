//! \file hdmi_dev.h
//! \brief Commands to interact with the HDMI Peripheral
//!
//! There is only one HDMI peripheral in existence - it is necessarily a
//! singleton. The methods here control that single peripheral.
//!
//! Note that these methods are not reentrant nor thread-safe.

#pragma once

#include <stdbool.h>

//! \brief Initialize the HDMI Peripheral
//!
//! This method flashes the peripheral onto the PL. It also configures the
//! clocks and allocates memory for the framebuffer. If this method fails, the
//! PL and the clocks will be in an undefined state, but no resources will be
//! leaked.
//!
//! \return Whether initialization was successful
bool hdmi_dev_open(void);

//! \brief Inverse of hdmi_dev_open
//!
//! This method frees all the host-side resources currently in use by the
//! peripheral. However, it leaves the PL and the clocks in the same state.
void hdmi_dev_close(void);
