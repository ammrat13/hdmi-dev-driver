CC := gcc
LD := gcc

CFLAGS := \
	-O2 -Wall -Wextra -Werror \
	-I./third-party/XRT/src/runtime_src/core/edge/include/

PROG := hdmi-dev-driver
OFILES := main.o fb.o hdmi_dev.o
DFILES := $(OFILES:.o=.d)

.PHONY: all
all: $(PROG)

.PHONY: clean
clean:
	rm -f $(PROG) $(OFILES) $(DFILES)

$(PROG): $(OFILES)
	$(LD) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

-include $(DFILES)
