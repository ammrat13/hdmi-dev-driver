#include "fb.h"

#include <fcntl.h>
#include <libdrm/drm.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <zynq_ioctl.h>

//! \brief The path to the device file used to allocate framebuffers
static const char *const DEV_FILE =
    "/dev/dri/by-path/platform-axi:zyxclmm_drm-render";

//! \brief Length of the buffer we allocate in bytes
static const size_t BUF_SIZE = 640u * 480u * 4u;

bool fb_allocator_init(fb_allocator_t *alloc) {
  // Edge cases
  if (alloc == NULL)
    return false;
  // Try to open the device file, and report whether it succeeded
  alloc->fd = open(DEV_FILE, O_RDWR);
  return alloc->fd != -1;
}

void fb_allocator_close(fb_allocator_t alloc) { close(alloc.fd); }

bool fb_allocate(fb_allocator_t alloc, fb_t *fb) {

  // Edge cases
  if (fb == NULL)
    return false;
  // Initialize everything to a known state
  fb->handle = 0;
  fb->physical_address = ~0u;
  fb->data = MAP_FAILED;

  // Try to allocate the buffer object
  {
    // Arguments
    struct drm_zocl_create_bo args = {
        .size = BUF_SIZE,
        .flags = DRM_ZOCL_BO_FLAGS_CMA,
    };
    // IOCTL call
    int res = ioctl(alloc.fd, DRM_IOCTL_ZOCL_CREATE_BO, &args);
    if (res == -1)
      goto failure;
    // Exfiltrate data
    fb->handle = args.handle;
  }

  // Try to get the physical address of the buffer object
  {
    // Arguments
    struct drm_zocl_info_bo args = {
        .handle = fb->handle,
    };
    // IOCTL call
    int res = ioctl(alloc.fd, DRM_IOCTL_ZOCL_INFO_BO, &args);
    if (res == -1)
      goto failure;
    // Exfiltrate data. We only have 32-bit addresses, so we can ignore the
    // higher bits. Also take this opportunity to do sanity checking - we
    // should've gotten the size we asked for.
    if (args.size != BUF_SIZE)
      goto failure;
    fb->physical_address = (intptr_t)args.paddr;
  }

  // Finally, try to map the buffer into our address space
  {
    // Arguments
    struct drm_zocl_map_bo args = {
        .handle = fb->handle,
    };
    // IOCTL call
    int res = ioctl(alloc.fd, DRM_IOCTL_ZOCL_MAP_BO, &args);
    if (res == -1)
      goto failure;
    // MMAP call
    fb->data = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                    alloc.fd, args.offset);
    if (fb->data == MAP_FAILED)
      goto failure;
  }

  // Done
  return true;

  // On failure, make sure to release all the handles we got. Also make sure to
  // reset the data we return to the user.
failure:
  fb_free(alloc, *fb);
  fb->handle = 0;
  fb->physical_address = ~0u;
  fb->data = MAP_FAILED;
  return false;
}

void fb_free(fb_allocator_t alloc, fb_t fb) {

  // If we have data mapped, unmap it. Again, we're casting away volatile, but
  // that's fine since we're freeing the buffer.
  if (fb.data != MAP_FAILED)
    munmap((void *)fb.data, BUF_SIZE);

  // The physical address is only for our bookkeeping and doesn't have any
  // resources attached to it.

  // Free the handle. It's a GEM object, so we just use the IOCTL to free those.
  if (fb.handle != 0) {
    struct drm_gem_close args = {.handle = fb.handle};
    ioctl(alloc.fd, DRM_IOCTL_GEM_CLOSE, &args);
  }
}
