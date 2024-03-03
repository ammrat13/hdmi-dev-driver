//! \file fb.h
//! \brief Functions to allocate and free framebuffers
//!
//! Framebuffers for the HDMI Peripheral are always 640x480 words, with the RGB
//! components in the least-significant three bytes. The reason we have this
//! module is because framebuffers must be contiguous in physical memory, and
//! thus have to be allocated via DRM.

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! \brief An object that can be used to allocate framebuffers
//!
//! In order to allocate a framebuffer, we first open the device file, then do
//! ioctls on it to do the allocation. This structure saves the file descriptor
//! from the call to `open`. That file descriptor is used during the actual
//! allocations.
typedef struct fb_allocator_t {
  int fd;
} fb_allocator_t;

//! \brief Initialize an `fb_allocator_t`
//! \details An allocator must be initialized before use
//! \param[out] alloc The allocator to initialize
//! \return Whether the initialization succeeded
bool fb_allocator_init(fb_allocator_t *alloc);
//! \brief Close an `fb_allocator_t`
//! \details An allocator cannot be used after it is closed
void fb_allocator_close(fb_allocator_t alloc);

//! \brief A single framebuffer
//!
//! Framebuffers always contain an array of 640x480 words, which are accessible
//! using `fb_ptr`. The structure also keeps the handle internal used by the
//! buffer. These framebuffers should be freed with the same allocator used to
//! create them.
//!
//! \see fb_ptr
typedef struct fb_handle_t {
  uint32_t handle;
  intptr_t physical_address;
  volatile uint32_t *volatile data;
} fb_handle_t;

//! \brief Get a pointer to the framebuffer's data
static inline uint32_t *fb_ptr(fb_handle_t fb) { return (uint32_t *)fb.data; }

//! \brief Allocate a framebuffer with an allocator
//! \return An `fb_handle_t` on the heap
fb_handle_t *fb_allocate(fb_allocator_t alloc);
//! \brief Free a framebuffer via its handle
//! \details The framebuffer handle should not be used after this
void fb_free(fb_allocator_t alloc, fb_handle_t *fb);
