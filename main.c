#include "video.h"

#include <stdio.h>

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "Error: wrong number of arguments");
    exit(1);
  }

  video_t *vid = video_open(argv[1]);
  printf("vid = %p\n", vid);
  video_close(vid);
}
