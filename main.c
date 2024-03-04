#include "video.h"

#include <stdio.h>

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "Error: wrong number of arguments");
    exit(1);
  }

  video_t *vid = video_open(argv[1]);
  if (vid == NULL) {
    fprintf(stderr, "Error: failed to open video\n");
    exit(1);
  }

  uint32_t *fb = calloc(640u * 480u, sizeof(uint32_t));
  int res;
  size_t count = 0;
  while ((res = video_get_frame(vid, fb)) == 0) {
    printf("Got frame %lu\n", count++);
  }
  printf("Bad %d %d\n", res, AVERROR_EOF);

  free(fb);
  video_close(vid);
}
