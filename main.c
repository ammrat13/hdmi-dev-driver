#include "video.h"

#include <stdio.h>

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "Error: wrong number of arguments\n");
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
    printf("Got frame %u\n", count++);

    // static char fname[1024];
    // snprintf(fname, sizeof(fname), "frame-%lu.pbm", count);

    // FILE *f = fopen(fname, "w");
    // fprintf(f, "P6\n640 480\n255\n");

    // for (size_t r = 0lu; r < 480lu; r++) {
    //   for (size_t c = 0lu; c < 640lu; c++) {
    //     uint32_t color = fb[r * 640lu + c];
    //     uint8_t r = (color >> 16) & 0xffu;
    //     uint8_t g = (color >>  8) & 0xffu;
    //     uint8_t b = (color >>  0) & 0xffu;
    //     uint8_t data[] = {r, g, b};
    //     fwrite(data, 3lu, 1lu, f);
    //   }
    // }
  }
  printf("Bad %d %d\n", res, AVERROR_EOF);

  free(fb);
  video_close(vid);
}
