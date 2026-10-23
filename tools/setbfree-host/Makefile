# Host-only tools that build the unmodified setBfree DSP. GPL-2.0-or-later.
# Not part of the firmware.

SETB ?= ../../third_party/setBfree
CC   ?= cc

CFLAGS ?= -O2 -g -ffp-contract=off -ffunction-sections -fdata-sections \
          -I. -I$(SETB)/src -I$(SETB)/b_whirl -I$(SETB)/b_overdrive -I$(SETB)/b_reverb
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif

SETB_SRC = $(SETB)/src/tonegen.c $(SETB)/src/vibrato.c \
           $(SETB)/b_whirl/whirl.c $(SETB)/b_whirl/eqcomp.c \
           $(SETB)/b_overdrive/overdrive.c $(SETB)/b_overdrive/filterTools.c \
           $(SETB)/b_reverb/reverb.c

BINS = refrender dump_tables

all: $(BINS)

refrender: refrender.c host.c $(SETB_SRC) host.h
	$(CC) $(CFLAGS) -o $@ refrender.c host.c $(SETB_SRC) $(LDFLAGS) -lm

dump_tables: dump_tables.c host.c $(SETB_SRC) host.h
	$(CC) $(CFLAGS) -o $@ dump_tables.c host.c $(SETB_SRC) $(LDFLAGS) -lm

check: all
	mkdir -p build
	./dump_tables --summary
	./refrender events/demo.txt build/demo.wav
	@echo "refrender: build/demo.wav"

clean:
	rm -rf build $(BINS)

.PHONY: all check clean
