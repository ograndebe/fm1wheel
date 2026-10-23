# WheelBox: host build, host tests, the setBfree reference and the device build.
# The device build needs Docker, the JieLi toolchain and the AC79 SDK; see README.md.

PY ?= ./.venv/bin/python
AC79_SDK ?= $(HOME)/fw-AC79_AIoT_SDK
JIELI_TOOLCHAIN ?= $(HOME)/.jieli/toolchain

.PHONY: all build-host test-host ref tables whirl-tables organ-test compare device install clean

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

whirl-tables:
	$(MAKE) -C tools/setbfree-host whirl-tables

organ-test:
	$(MAKE) -C tools/organ-test check

compare:
	$(MAKE) -C tools/organ-test compare

device:
	AC79_SDK="$(AC79_SDK)" JIELI_TOOLCHAIN="$(JIELI_TOOLCHAIN)" $(PY) fm1-base/tools/build.py app

install: device
	$(PY) fm1-base/tools/fm1_install.py app/build/wheelbox.fwsc

clean:
	rm -rf app/build
	$(MAKE) -C tools/setbfree-host clean
