//! \file fb.h
//! \brief Functions to allocate and free framebuffers
//!
//! Framebuffers for the HDMI Peripheral are always 640x480 words, with the RGB
//! components in the least-significant three bytes. The reason we have this
//! module is because framebuffers must be contiguous in physical memory, and
//! thus have to be allocated via DRM.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! \brief An object that can be used to allocate framebuffers
//!
//! In order to allocate a framebuffer, we first open the device file, then do
//! ioctls on it to do the allocation. This structure saves the file descriptor
//! from the call to `open`. That file descriptor is used during the actual
//! allocations.
typedef struct hdmi_fb_allocator_t {
  int fd;
} hdmi_fb_allocator_t;

//! \brief Create an `hdmi_fb_allocator_t`
//! \return A pointer to the allocator on the heap, or `NULL` on failure
hdmi_fb_allocator_t *hdmi_fb_allocator_open(void);
//! \brief Close an `hdmi_fb_allocator_t`
//!
//! This is a `free` operation - don't use `alloc` after this. However, it is
//! legal to free a `NULL` allocator.
void hdmi_fb_allocator_close(hdmi_fb_allocator_t *alloc);

//! \brief A single framebuffer
//!
//! Framebuffers always contain an array of 640x480 words, which are accessible
//! using `hdmi_fb_ptr`. The structure also keeps the handle internal used by
//! the buffer. These framebuffers should be freed with the same allocator used
//! to create them.
//!
//! \see hdmi_fb_ptr
typedef struct hdmi_fb_handle_t {
  uint32_t handle;
  intptr_t physical_address;
  volatile uint32_t *volatile data;
} hdmi_fb_handle_t;

//! \brief Get a pointer to the framebuffer's data
static inline uint32_t *hdmi_fb_data(hdmi_fb_handle_t *fb) {
  if (fb == NULL)
    return NULL;
  return (uint32_t *)fb->data;
}

//! \brief Allocate a framebuffer with an allocator
//!
//! It is legal for the `alloc` parameter to be `NULL`. In that case, allocation
//! always fails and returns `NULL`.
//!
//! \param[in] alloc The allocator to use
//! \return An `hdmi_fb_handle_t` on the heap, or `NULL` on failure
hdmi_fb_handle_t *hdmi_fb_allocate(hdmi_fb_allocator_t *alloc);
//! \brief Free a framebuffer via its handle
//!
//! The framebuffer handle should not be used after this, as it is a free
//! operation.
//!
//! Additionally, if the `fb` is not `NULL`, then the `alloc` parameter must not
//! be `NULL`. If it is, the framebuffer will not be freed and this function is
//! a no-op.
void hdmi_fb_free(hdmi_fb_allocator_t *alloc, hdmi_fb_handle_t *fb);

//! \brief Flush a framebuffer's contents from the cache
//!
//! This must be called before giving the framebuffer to the HDMI Peripheral.
//! Otherwise, the device will read stale data.
//!
//! This function is a no-op if `fb` or `alloc` is `NULL`.
void hdmi_fb_flush(hdmi_fb_allocator_t *alloc, hdmi_fb_handle_t *fb);
