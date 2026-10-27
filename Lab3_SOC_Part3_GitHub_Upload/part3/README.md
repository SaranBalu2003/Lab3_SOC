# Lab 3 Part 3: SystemC Q8_0 accelerator

This package models the selected alternate Lab 2 accelerator and integrates it into the uploaded, working `zynqmp_demo` platform. It includes a standalone numerical check, SystemC/TLM tests, a guarded installer, and a small ARM MMIO test.

## Current evidence

The selected **128-block HLS kernel** has already passed C simulation and Verilog C/RTL co-simulation on mario. The supplied reports show a 10 ns target, achieved block II=1, HLS maximum latency 141 cycles, observed RTL maximum latency 138 cycles, and 3 DSP / 1708 FF / 6361 LUT. C simulation passed 27 original and 410 added cases, with zero ULP error against the exact-sum oracle for the added cases.

The new **portable functional core** was checked locally against 410 golden vectors (zero ULP), all 63,488 finite FP16 scale encodings in a scaled one-block test, invalid inputs, and undefined-behavior sanitization. The SystemC/TLM model and integrated QEMU platform subsequently passed on mario: `make test` reported zero failures; `zynqmp_demo` built; ARM reads returned the expected ID/capacity; and the direct-MMIO test passed at 0, 1, 3, and 128 blocks. See [TEAM_HANDOFF.md](TEAM_HANDOFF.md) for the recorded outputs and current status. Accelerator-specific Linux driver/IRQ delivery and llama.cpp integration remain Part 4 work.

## Files

| File | Purpose |
| --- | --- |
| `dot_math.h/.cpp` | Exact fixed-point functional behavior without Xilinx headers |
| `dot_accel.h/.cpp` | SystemC process, local storage, TLM target, timing, reset and IRQ |
| `dot_regs.h` | Shared MMIO constants and little-endian helpers |
| `test_math.cpp` | Native numerical and input-contract checks |
| `test_dot_accel.cpp` | Standalone SystemC numerical/protocol/timing/reset/IRQ checks |
| `fixtures.h`, `golden_cases.h` | The 410 deterministic cases already used in HLS validation |
| `Makefile` | Host tests and ARM MMIO-test build |
| `mmio_smoke.c` | Guest Linux direct-MMIO test, before Part 4 driver integration |
| `platform/` | Modified versions of the uploaded top-level source and Makefile |
| `platform_changes.patch` | Reviewable integration diff |
| `install_platform.py` | Hash-checked installation with backups |
| `upstream/` | Uploaded platform files, selected kernel source, and supplied result evidence |
| `hls_validation/` | Expanded 128-block HLS kernel, independent oracle, tests, and reproduction scripts |
| `TEAM_HANDOFF.md` | Completed validation results and Part 4 starting point |

## Step 1: standalone tests on mario

This repository stores the model in `part3/`. Commands below record the already-tested working copy at `/misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc`; when using a checkout, substitute your checkout's `part3/` directory. In a **host terminal**, run:

```bash
cd /misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc
module load systemc/2.3.4
make test
```

Expected summary markers:

```text
MATH PASS: 410 golden vectors (0 ULP), 63488 finite-half scale cases, and invalid-input checks.
SYSTEMC TESTS PASSED: 410 golden vectors plus protocol/timing/reset checks; failures=0
```

Use the native host compiler for SystemC, not the ARM cross-compiler. `HOST_CXX` defaults to `g++`. The build keeps the compiler's default language standard, matching the working demo's build convention. SystemC paths default to `/usr/local/packages/systemc-2.3.4`; override `SYSTEMC` and `SYSTEMC_LIBDIR` if needed.

The SystemC test covers the full buffers, register behavior, endian handling, incoming delay, no DMI, malformed accesses, length errors, nonfinite-scale errors, writes/restarts/ACK while busy, pending completion, reset cancellation, post-reset reuse, IRQ enable/disable/acknowledgement, the modeled completion timestamp, and all 410 golden vectors.

## Step 2: install into the existing platform

After the standalone tests pass, cleanly shut down the active QEMU Linux guest, exit QEMU, and stop the old SystemC simulator before rebuilding/restarting the platform.

From the package directory:

```bash
python3 install_platform.py /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
cd /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
module load systemc/2.3.4
make zynqmp_demo
```

The installer verifies the two platform files against the exact uploaded originals or this package's already-installed versions. It preserves originals in `lab3_before_dot/`, copies the accelerator files, and installs the reviewed source/Makefile changes. It can be rerun with the identical package. It stops before changes if unrelated edits or conflicting model files are found.

The integration:

- Adds one target to the existing interconnect.
- Maps the accelerator at `0xa1000000-0xa100ffff`.
- Connects its active-high reset to the platform's `rst` signal.
- Connects its level interrupt to `zynq.pl2ps_irq[5]`. Existing debug/DMA lines use indices 0-4.
- Adds `dot_accel.o` and `dot_math.o` to the ZynqMP target only.
- Retains the existing debug device, scratchpad, DMA examples, and disabled VCD tracing.

The actual Linux interrupt mapping and driver/device-tree entry for this new accelerator will be set up in Part 4. The tutorial's existing driver remains associated with its original debug device; loading it does not automatically drive this accelerator.

To restore the original platform source/build configuration if needed:

```bash
cd /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
cp lab3_before_dot/zynqmp_demo.cc .
cp lab3_before_dot/Makefile .
make zynqmp_demo
```

## Step 3: restart QEMU/SystemC

Terminal 1 on mario:

```bash
cd /misc/scratch/sb69942/soc/Lab3
/home/projects/gerstl/ece382m/qemu-boot -c
```

Terminal 2 on the same host:

```bash
cd /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
module load systemc/2.3.4
./zynqmp_demo unix:/misc/scratch/sb69942/soc/Lab3/tmp/qemu-rport-_amba@0_cosim@0 1000000
```

Log in as `petalinux` in Terminal 1. Probe the new peripheral:

```bash
sudo peek 0xa1000018
sudo peek 0xa1000020
```

Expected ID: `0x51384450`; expected capacity: `0x00001000` (4096 elements). The ID distinguishes this accelerator from the original tutorial debug device.

## Step 4: ARM direct-MMIO smoke test

In a host terminal with the tutorial's ARM compiler environment loaded:

```bash
cd /misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc
make arm-smoke
```

Inside QEMU Linux:

```bash
scp sb69942@mario.ece.utexas.edu:/misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc/dot_mmio_smoke .
sudo ./dot_mmio_smoke
```

This test fills both local buffers using 32-bit CPU writes and runs all-ones vectors of 0, 1, 3, and 128 blocks. Expected results are 0, 32, 96, and 4096; the final marker is `QEMU MMIO SMOKE TEST PASSED`. It uses polling and `/dev/mem`; it is a Part 3 validation utility. Part 4 still requires the accelerator-specific HAL/kernel driver and llama.cpp integration.

## Register and storage interface

All register offsets below are relative to `0xa1000000`. Registers require aligned 32-bit accesses and are little-endian. Byte enables, streaming bursts, unmapped locations, and writes to read-only registers return TLM errors.

| Offset | Register | Behavior |
| --- | --- | --- |
| `0x00` | CONTROL | Write 1 to START; write 2 to ACK completion; reads return 0 |
| `0x04` | STATUS | bit 0 BUSY, bit 1 DONE, bit 2 ERROR |
| `0x08` | N_ELEMENTS | Vector element count, a multiple of 32, 0-4096 |
| `0x0c` | RESULT | Returned FP32 bit pattern; valid after DONE with no ERROR |
| `0x10` | ERROR | 0 success, 1 invalid length, 2 nonfinite active scale |
| `0x14` | IRQ_ENABLE | Write 0 or 1; IRQ is high while enabled and DONE is set |
| `0x18` | ID | `0x51384450` |
| `0x1c` | CYCLES | Modeled cycles for the last accepted valid-length command |
| `0x20` | CAPACITY | Maximum vector length: 4096 elements |
| `0x1000-0x2fff` | X buffer | 128 slots of 64 bytes |
| `0x3000-0x4fff` | Y buffer | 128 slots of 64 bytes |

Each input slot contains:

| Bytes within slot | Contents |
| --- | --- |
| 0-1 | Raw FP16 scale, little-endian |
| 2-33 | 32 signed int8 quantized values |
| 34-63 | Padding, ignored by computation |

The padding matches reserving a 512-bit memory word per Q8_0 block. The bus-visible buffers occupy **16384 bytes total**, well within the lab's local-memory limit. Packed useful input data occupies 8704 bytes. The TLM process uses the same resident buffers rather than an additional full-size snapshot.

Protocol:

1. ACK any previous completion, then fill inputs and set N_ELEMENTS.
2. Optionally enable the IRQ; write START.
3. While BUSY, input/configuration writes, START, and ACK return errors. Status/result/buffer reads remain available; RESULT is valid only after successful completion.
4. Poll DONE or wait for the new accelerator's IRQ. Check ERROR, then read RESULT.
5. ACK clears DONE/ERROR and deasserts the IRQ. A new START is accepted after ACK. Result bits are retained until the next START or reset.

Invalid lengths complete immediately with an error. Nonfinite scales within the active vector complete with error after the modeled delay. Unused-tail scales are not inspected. Active-high reset cancels outstanding work, clears buffers/registers, disables interrupts, and prevents stale completion. IRQ writes occur in one SC_METHOD to avoid multiple-writer errors on normal SystemC signals.

## Arithmetic and timing model

The functional core preserves exact significand/exponent multiplication and accumulation from the selected HLS implementation, followed by binary32 nearest-even conversion. For at most 128 blocks, the raw fixed-point sum magnitude is below 2^106, so the native GCC signed 128-bit type is sufficient even though the original arbitrary-length HLS code uses a 129-bit accumulator. No contribution is rounded to FP32 before accumulation. Exact zero returns positive zero. The 410 golden results matched the HLS C simulation exactly; broader bitwise equivalence outside the verified finite-input/capacity contract is not claimed.

The **estimated computation delay** is:

```text
modeled_cycles = n_blocks + 13
modeled_delay = modeled_cycles * 10 ns
```

This is a conservative length-dependent estimate inferred from achieved block II=1 and the HLS maximum of 141 cycles at 128 blocks: `128+13=141`. It intentionally exceeds the observed full-suite RTL maximum of 138 cycles at the longest supported length. It is not a measured per-input latency curve; shorter or zero vectors can complete earlier in RTL. Document it as an estimate in the report, and refine it with per-length RTL transaction evidence if more accurate timing is needed.

Each valid blocking TLM access consumes its incoming delay plus an assumed 10 ns service time; this parameter is a bus-model assumption, not a board measurement. The worker runs asynchronously and waits separately for the computation delay. CPU data-transfer and synchronization operations therefore contribute their own simulated time. DMI is disabled. The standalone test can send large buffer bursts for fixture loading; the supplied ARM smoke test uses individual 32-bit MMIO writes.

## Submission and remaining work

Keep this model, tests, platform sources/diff, build instructions, and selected HLS evidence with the eventual `part3/` submission directory. This package's working directory is named `lab3_part3_systemc` to avoid replacing existing work; organize it under `part3/` when assembling the final repository.

Standalone SystemC tests, the platform build, and QEMU direct-MMIO interaction have passed on mario. Part 4 will add a kernel driver/device-tree description for this accelerator, a drop-in HAL, and llama.cpp/Tiny-Stories validation. DMA/bus mastering is not implemented in this basic slave accelerator.
