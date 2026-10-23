# WheelBox: host build, host tests, the setBfree reference and the device build.
# The device build needs Docker, the JieLi toolchain and the AC79 SDK; see README.md.

PY ?= ./.venv/bin/python

.PHONY: all build-host test-host ref tables organ-test compare device clean

all: build-host ref

build-host:
	$(PY) fm1-base/tools/build.py app --host

test-host: build-host
	./app/build/host/wheelbox_host app/tests/basic.txt app/build/host

ref:
	$(MAKE) -C tools/setbfree-host check

tables:
	$(MAKE) -C tools/setbfree-host dump_tables
	./tools/setbfree-host/dump_tables --header app/dsp/organ_tables.h --summary

organ-test:
	$(MAKE) -C tools/organ-test check

compare:
	$(MAKE) -C tools/organ-test compare

device:
	$(PY) fm1-base/tools/build.py app

clean:
	rm -rf app/build
	$(MAKE) -C tools/setbfree-host clean
