# Host SystemC build; uses the same compiler default standard as zynqmp_demo.
HOST_CXX ?= g++
SYSTEMC ?= /usr/local/packages/systemc-2.3.4
SYSTEMC_LIBDIR ?= $(SYSTEMC)/lib-linux64
CXXFLAGS ?= -O2 -g -Wall -Wextra -ffp-contract=off
SC_FLAGS = -I$(SYSTEMC)/include
SC_LIBS = -L$(SYSTEMC_LIBDIR) -Wl,-rpath,$(SYSTEMC_LIBDIR) -lsystemc -pthread

.PHONY: all test clean arm-smoke
all: test_dot_accel
test_math: test_math.cpp dot_math.cpp dot_math.h dot_regs.h fixtures.h golden_cases.h
	$(HOST_CXX) $(CXXFLAGS) test_math.cpp dot_math.cpp -o $@
test_dot_accel: test_dot_accel.cpp dot_accel.cpp dot_math.cpp dot_accel.h dot_math.h dot_regs.h fixtures.h golden_cases.h
	$(HOST_CXX) $(CXXFLAGS) $(SC_FLAGS) test_dot_accel.cpp dot_accel.cpp dot_math.cpp -o $@ $(SC_LIBS)
test: test_math test_dot_accel
	./test_math
	./test_dot_accel
arm-smoke: mmio_smoke.c dot_regs.h
	aarch64-linux-gnu-gcc -O2 -Wall -Wextra mmio_smoke.c -o dot_mmio_smoke
clean:
	rm -f test_math test_dot_accel dot_mmio_smoke
