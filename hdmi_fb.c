#include "hdmi_fb.h"

#include <fcntl.h>
#include <libdrm/drm.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <zynq_ioctl.h>

//! \brief The path to the device file used to allocate framebuffers
static const char *const DEV_FILE =
    "/dev/dri/by-path/platform-axi:zyxclmm_drm-render";

//! \brief Length of the buffer we allocate in bytes
static const size_t BUF_SIZE = 640u * 480u * 4u;

hdmi_fb_allocator_t *hdmi_fb_allocator_open(void) {
  // Try to allocate the return value
  hdmi_fb_allocator_t *ret = calloc(1u, sizeof(hdmi_fb_allocator_t));
  if (ret == NULL)
    return NULL;
  // Try to open the file
  ret->fd = open(DEV_FILE, O_RDWR);
  if (ret->fd == -1) {
    free(ret);
    return NULL;
  }
  // We succeeded on both counts
  return ret;
}

void hdmi_fb_allocator_close(hdmi_fb_allocator_t *alloc) {
  // Make sure this works even if `alloc` is only partially initialized
  if (alloc != NULL) {
    if (alloc->fd != -1)
      close(alloc->fd);
    free(alloc);
  }
}

hdmi_fb_handle_t *hdmi_fb_allocate(hdmi_fb_allocator_t *alloc) {

  // Edge case handling
  if (alloc == NULL)
    return NULL;

  // Allocate space for the return value
  hdmi_fb_handle_t *ret = calloc(1u, sizeof(hdmi_fb_handle_t));
  if (ret == NULL)
    return NULL;
  // Initialize everything to a known state
  ret->handle = 0;
  ret->physical_address = ~0u;
  ret->data = MAP_FAILED;

  // Try to allocate the buffer object
  {
    // Arguments
    struct drm_zocl_create_bo args = {
        .size = BUF_SIZE,
        .flags = DRM_ZOCL_BO_FLAGS_CMA,
    };
    // IOCTL call
    int res = ioctl(alloc->fd, DRM_IOCTL_ZOCL_CREATE_BO, &args);
    if (res == -1)
      goto failure;
    // Exfiltrate data
    ret->handle = args.handle;
  }

  // Try to get the physical address of the buffer object
  {
    // Arguments
    struct drm_zocl_info_bo args = {
        .handle = ret->handle,
    };
    // IOCTL call
    int res = ioctl(alloc->fd, DRM_IOCTL_ZOCL_INFO_BO, &args);
    if (res == -1)
      goto failure;
    // Exfiltrate data. We only have 32-bit addresses, so we can ignore the
    // higher bits. Also take this opportunity to do sanity checking - we
    // should've gotten the size we asked for.
    if (args.size != BUF_SIZE)
      goto failure;
    ret->physical_address = (intptr_t)args.paddr;
  }

  // Finally, try to map the buffer into our address space
  {
    // Arguments
    struct drm_zocl_map_bo args = {
        .handle = ret->handle,
    };
    // IOCTL call
    int res = ioctl(alloc->fd, DRM_IOCTL_ZOCL_MAP_BO, &args);
    if (res == -1)
      goto failure;
    // MMAP call
    ret->data = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                     alloc->fd, args.offset);
    if (ret->data == MAP_FAILED)
      goto failure;
  }

  // Done
  return ret;

  // On failure, make sure to release all the handles we got
failure:
  hdmi_fb_free(alloc, ret);
  return NULL;
}

void hdmi_fb_free(hdmi_fb_allocator_t *alloc, hdmi_fb_handle_t *fb) {
  // Note that this function may be called from `fb_alloc`, meaning the data may
  // be partially initialized. We should be tolerant of that.

  // Edge case handling
  if (alloc == NULL || fb == NULL)
    return;

  // If we have data mapped, unmap it. Again, we're casting away volatile, but
  // that's fine since we're freeing the buffer.
  if (fb->data != MAP_FAILED)
    munmap((void *)fb->data, BUF_SIZE);

  // The physical address is only for our bookkeeping and doesn't have any
  // resources attached to it.

  // Free the handle. It's a GEM object, so we just use the IOCTL to free those.
  if (fb->handle != 0) {
    struct drm_gem_close args = {.handle = fb->handle};
    ioctl(alloc->fd, DRM_IOCTL_GEM_CLOSE, &args);
  }

  // The pointer itself is allocated on the heap, so free it
  free(fb);
}
