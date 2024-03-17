CC := gcc
LD := gcc

CFLAGS := \
	-O2 -flto -Wall -Wextra -Werror \
	-I./third-party/XRT/src/runtime_src/core/edge/include/
LFLAGS := -lavcodec -lavformat -lavutil -lswscale -flto

PROG := hdmi-dev-video-player
OFILES := main.o hdmi_fb.o hdmi_dev.o video.o
DFILES := $(OFILES:.o=.d)

.PHONY: all
all: $(PROG)

.PHONY: clean
clean:
	rm -f $(PROG) $(OFILES) $(DFILES)

$(PROG): $(OFILES)
	$(LD) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

-include $(DFILES)
