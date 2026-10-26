# fm1-wheel: host build, host tests, the setBfree reference and the device builds.
# The device build needs Docker, the JieLi toolchain and the AC79 SDK; see README.md.
#
# Two boards, one app and one builder:
#   make fm1       FM-1 (the default; byte-identical to the FM-1-only build)
#   make smk37     M-VAVE SMK-37 Pro
#   make site      both packages and the static site

PY ?= ./.venv/bin/python
AC79_SDK ?= $(HOME)/fw-AC79_AIoT_SDK
JIELI_TOOLCHAIN ?= $(HOME)/.jieli/toolchain
VERSION ?= 0.2.0
MODEL ?= pro

.PHONY: all build-host build-host-smk test-host test-host-smk web web-smk serve-web site \
        ref tables whirl-tables organ-test compare fm1 smk37 device install install-fm1 install-smk37 clean

all: build-host ref

build-host:
	$(PY) fm1-base/tools/build.py app --host

build-host-smk:
	$(PY) fm1-base/tools/build.py app --board smk37 --model $(MODEL) --host

test-host: build-host
	./app/build/host/fm1-wheel_host app/tests/basic.txt app/build/host

test-host-smk: build-host-smk
	mkdir -p app/build/smk37/test-out
	./app/build/smk37/host/fm1-wheel-smk37_host app/tests/smk37.txt app/build/smk37/test-out

web:
	$(PY) fm1-base/tools/build.py app --web

web-smk:
	$(PY) fm1-base/tools/build.py app --board smk37 --model $(MODEL) --web

serve-web: web
	@echo "open http://localhost:8790/"
	$(PY) -m http.server -d app/build/emu 8790

site:
	AC79_SDK="$(AC79_SDK)" JIELI_TOOLCHAIN="$(JIELI_TOOLCHAIN)" $(PY) tools/site/build_site.py --version $(VERSION)

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

fm1:
	AC79_SDK="$(AC79_SDK)" JIELI_TOOLCHAIN="$(JIELI_TOOLCHAIN)" $(PY) fm1-base/tools/build.py app --board fm1

smk37:
	AC79_SDK="$(AC79_SDK)" JIELI_TOOLCHAIN="$(JIELI_TOOLCHAIN)" $(PY) fm1-base/tools/build.py app --board smk37 --model $(MODEL)

device: fm1

install: install-fm1

install-fm1: fm1
	$(PY) fm1-base/tools/fm1_install.py app/build/fm1-wheel.fwsc

install-smk37: smk37
	$(PY) fm1-base/tools/fm1_install.py app/build/smk37/fm1-wheel-smk37.fwsc

clean:
	rm -rf app/build
	$(MAKE) -C tools/setbfree-host clean
