# dubdelay research prototypes. `make` builds the host tools; `make all-results` regenerates renders and docs;
# `make fm1` cross-compiles the DSP for the FM-1 (pi32v2) with Orange Cat's flags (compile only, nothing is flashed).
CC ?= cc
DSPFLAGS = -std=c99 -O2 -Wall -Wextra -ffp-contract=off -Werror=double-promotion
JIELI_TOOLCHAIN ?= $(HOME)/fm1-toolchain/jieli/jieli-linux-toolchains-20260730.1
PI32FLAGS = -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Werror=double-promotion -ffunction-sections -fno-builtin \
            -ffreestanding -std=c99 -Wall -Wextra

all: build/render build/measure build/bench
build/dd.o: src/dubdelay.c src/dubdelay.h src/dd_math.h | build
	$(CC) $(DSPFLAGS) -c $< -o $@
build/render: tools/render.c build/dd.o src/dd_scenes.h
	$(CC) $(DSPFLAGS) -Wno-unused-function -o $@ tools/render.c build/dd.o -lm
build/measure: tools/measure.c build/dd.o src/dd_scenes.h
	$(CC) -std=gnu99 -O2 -ffp-contract=off -Wno-unused-function -o $@ tools/measure.c build/dd.o -lm
build/bench: tools/bench.c build/dd.o src/dd_scenes.h
	$(CC) $(DSPFLAGS) -Wno-unused-function -o $@ tools/bench.c build/dd.o -lm
build:
	mkdir -p build renders
all-results: all
	./build/render renders > docs/RENDERS.csv
	./build/measure > docs/MEASUREMENTS.md
	./build/bench > docs/BENCH_HOST.md
fm1: | build
	$(JIELI_TOOLCHAIN)/pi32v2/bin/clang $(PI32FLAGS) -O2 -c src/dubdelay.c -o build/dubdelay_pi32.o
	$(JIELI_TOOLCHAIN)/pi32v2/bin/clang $(PI32FLAGS) -O2 '-DDD_NOINL=__attribute__((noinline))' -c src/dubdelay.c -o build/dd_noinl.o
	$(JIELI_TOOLCHAIN)/common/bin/objdump -d -mcpu=r3 -mattr=+fprev1 build/dd_noinl.o > build/dd_noinl.dis
	python3 -I tools/loops.py build/dd_noinl.dis
clean:
	rm -rf build
.PHONY: all all-results fm1 clean
