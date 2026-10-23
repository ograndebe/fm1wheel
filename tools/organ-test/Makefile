# Host renderer for the WheelBox organ engine. GPL-3.0-or-later.
CC ?= cc
PY ?= ../../.venv/bin/python
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -ffp-contract=off \
          -I../../app/dsp -I../../fm1-base/include
TABLES = ../../app/dsp/organ_tables.h

all: render

$(TABLES):
	$(MAKE) -C ../setbfree-host dump_tables
	../setbfree-host/dump_tables --header $(TABLES)

render: render.c ../../app/dsp/organ.c ../../app/dsp/vibrato.c ../../app/dsp/preamp.c ../../app/dsp/reverb.c ../../app/dsp/organ.h ../../app/dsp/vibrato.h ../../app/dsp/preamp.h ../../app/dsp/reverb.h $(TABLES)
	$(CC) $(CFLAGS) -o $@ render.c ../../app/dsp/organ.c ../../app/dsp/vibrato.c ../../app/dsp/preamp.c ../../app/dsp/reverb.c -lm

check: render
	mkdir -p build
	for f in ../setbfree-host/events/*.txt; do \
		./render $$f build/$$(basename $$f .txt).wav || exit 1; \
	done

compare: check
	$(MAKE) -C ../setbfree-host all
	mkdir -p build/ref
	for f in ../setbfree-host/events/*.txt; do \
		name=$$(basename $$f .txt); \
		../setbfree-host/refrender $$f build/ref/$$name.wav --reverb >/dev/null || exit 1; \
		$(PY) compare.py build/$$name.wav build/ref/$$name.wav; \
	done

clean:
	rm -rf build render

.PHONY: all check compare clean
