# ECE382M Lab 3 - Part 3 Completion and Team Handoff

**Prepared for:** Saran (Saravanakrishnan B, sb69942) and teammates  
**Progress recorded:** October 24, 2026 (Austin time)  
**Host:** mario.ece.utexas.edu  
**Lab root:** `/misc/scratch/sb69942/soc/Lab3`

## 1. Where we stopped

**Part 3's accelerator model, platform integration, standalone tests, and basic ARM/QEMU computation test are complete and passing. Part 4 has not started.**

The last successful action was running `dot_mmio_smoke` inside QEMU Linux. It transferred input vectors to the new SystemC accelerator, started computation, polled completion, and checked returned results for lengths through 128 blocks.

| Work item | Verified status |
| --- | --- |
| Tutorial setup and examples through Section 3(e) | Previously completed |
| Select alternate Lab 2 accelerator and expand to 128 blocks | Completed |
| Vitis C simulation | 27 original + 410 extended cases passed |
| Vitis synthesis | Block II=1 retained at 10 ns target |
| Verilog C/RTL co-simulation | Pass |
| Portable functional-core tests | 410 golden vectors at 0 ULP; 63,488 finite-half scale cases passed |
| Standalone SystemC tests | 410 golden vectors and protocol/timing/reset/IRQ checks passed; 0 failures |
| Installation and rebuild of `zynqmp_demo` | Completed |
| ARM reads accelerator ID/capacity | Expected values returned |
| ARM direct-MMIO computation test | Passed at 0, 1, 3, and 128 blocks |
| New accelerator interrupt through a Linux driver | Not yet tested; Part 4 work |
| New accelerator HAL/driver and llama.cpp integration | Not started |

These mario results supersede the earlier package README notes that marked SystemC/QEMU validation as pending. This is a progress/handoff note, not a confirmation of final repository or Canvas submission.

## 2. What Part 3 implements

QEMU runs ARM Linux. A separate `zynqmp_demo` process on mario models FPGA-side devices in SystemC. The ARM accesses our accelerator using memory-mapped reads/writes carried over the QEMU/SystemC connection and the existing TLM interconnect.

For a dot product, ARM software fills two local input buffers, sets the element count, writes START, waits for DONE, and reads an FP32 result. The accelerator also exposes a completion interrupt for the future driver.

The physical group Ultra96 board was not used for these simulation tasks.

## 3. Selected Lab 2 architecture and expanded validation

We initially considered `part6/design_3` from Lab 2, then inspected the teammate's newer `alternate_optimal_design` in the uploaded repository archive. We selected the alternate design after confirming the achieved block II in the synthesis report.

| Metric | Lab 2 Design 3 | Original alternate |
| --- | ---: | ---: |
| Target period | 7.5 ns | 10 ns |
| Achieved block II | 4 cycles | 1 cycle |
| Time between block launches | 30 ns | 10 ns |
| DSP | 19 | 3 |
| FF | 1706 | 1645 |
| LUT | 2828 | 6440 |
| Largest tested vector at that point | 128 blocks | 8 blocks |

The alternate offers 3x the steady block-launch rate at these clock targets, assuming adequate local-memory bandwidth. This is not a measured full-LLM speedup. Its original 8-block latency cannot be directly compared with a maximum over 128-block tests.

### Why the alternate achieves II=1

- Fully unrolls the 32-element integer dot product.
- Uses 512-bit input-memory ports.
- Decodes finite FP16 scales into sign, significand, and exponent.
- Accumulates exact fixed-point contributions in a 129-bit accumulator.
- Converts the final accumulated value to FP32 once.

This differs from rounding contributions and accumulating sequentially in FP32. For example, contributions `2^25, 1, -2^25` produce 1 with exact accumulation but 0 with sequential FP32. Numerical comparisons in Part 4 must account for the selected arithmetic.

### Changes for our 128-block capacity

In the separate `lab3_alternate_128_validation` project:

- Set `DOT_MAX_BLOCKS=128`: up to 4096 elements per input.
- Changed both input-interface depths from 8 to 128.
- Added a capacity assertion.
- Preserved the alternate's fixed-point arithmetic and optimization pragmas.
- Expanded the original testbench backing arrays to match depth=128 while preserving its original 27 test cases.
- Added 410 cases: 60 directed and 350 deterministic random tests, including subnormals, signed finite scales, cancellation, boundary lengths, last-element access, and ignoring unused tails.

The Python reference generator uses exact rational arithmetic. The expanded HLS C simulation reported:

```text
27 test cases passed.
Extended tests: 410/410 passed
Maximum ULP error against exact-sum FP32 oracle: 0
Outputs differing from sequential FP32 reference: 164/410
ALL VALIDATION TESTS PASSED: original 27 tests plus 410 extended tests
CSim done with 0 errors.
```

The 164 differences are expected from the changed accumulation method and were not errors against the exact-sum reference.

### Verified expanded-kernel HLS/RTL results

| Metric | Result |
| --- | ---: |
| FPGA | xczu3eg-sbva484-1-e |
| Vitis HLS | 2022.2 |
| Clock target | 10 ns |
| Block loop | `VITIS_LOOP_167_1` |
| Achieved block II | 1 cycle |
| Block iteration latency | 4 cycles |
| HLS maximum function latency | 141 cycles = 1.41 us |
| HLS maximum function interval | 142 cycles |
| HLS estimated slack | +0.06 ns |
| DSP / FF / LUT | 3 / 1708 / 6361 |
| Verilog co-simulation | Pass |
| Observed RTL latency min / avg / max | 5 / 42 / 138 cycles |
| Observed RTL transaction interval min / avg / max | 6 / 43 / 139 cycles |
| Total reported RTL execution | 19135 cycles |

These are HLS estimates and RTL simulation observations, not physical-board or post-route measurements. Block II describes iterations within a vector; it does not mean one complete vector can start every cycle.

Evidence on mario:

```text
/misc/scratch/sb69942/soc/Lab3/lab3_alternate_128_validation/results/
```

Relevant files: `validation_csim.log`, `validation_all.log`, `extended_csim_results.csv`, `csynth_summary.rpt`, `alternate128_csynth.rpt`, and `alternate128_cosim.rpt`.

## 4. SystemC model and file roles

Working directory:

```text
/misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc
```

| File | Purpose |
| --- | --- |
| `dot_math.h`, `dot_math.cpp` | Portable exact arithmetic and final FP32 conversion |
| `dot_accel.h`, `dot_accel.cpp` | SystemC worker, TLM interface, buffers, reset, status, timing and IRQ |
| `dot_regs.h` | Shared addresses, control/status bits and byte-order helpers |
| `fixtures.h`, `golden_cases.h` | Deterministic inputs and 410 exact expected outputs |
| `test_math.cpp` | Numerical, finite-half-scale and invalid-input checks |
| `test_dot_accel.cpp` | Standalone SystemC/TLM correctness and control tests |
| `mmio_smoke.c` | ARM Linux direct-MMIO computation test |
| `Makefile` | Host tests and ARM smoke-test build |
| `install_platform.py` | Hash-checked installation and backups |
| `platform/` | Modified platform top-level source and Makefile |
| `platform_changes.patch` | Reviewable integration diff |
| `upstream/` | Original platform/kernel sources and selected result evidence |
| `README.md` | Detailed implementation and run instructions |

### Arithmetic

The SystemC functional core removes the Xilinx `half`/`ap_fixed` dependencies while preserving exact scale/product accumulation for the supported finite-input contract. It uses native signed 128-bit integers: the enforced 128-block limit keeps the raw fixed-point sum magnitude below `2^106`, so this is sufficient despite the original arbitrary-length kernel using a 129-bit accumulator. Final FP32 conversion is nearest-even. Exact-zero results return positive zero.

### Local storage

There are two 8192-byte buffers, **16 KiB total**. Each vector has 128 slots of 64 bytes, matching a reserved 512-bit local-memory word per block:

| Bytes within a slot | Contents |
| --- | --- |
| 0-1 | Raw FP16 scale bits, little-endian |
| 2-33 | 32 signed int8 quantized values |
| 34-63 | Padding ignored by computation |

Useful packed input data is 8704 bytes for both vectors; the modeled storage allocation is 16384 bytes. This is within the lab's 216 x 36-kbit BRAM budget. Zero internal BRAM in an HLS compute-kernel report does not eliminate the need for external/local input buffers.

## 5. Register map and execution protocol

**Physical base:** `0xa1000000`  
**Mapped region:** `0xa1000000-0xa100ffff`  
**Interrupt connection:** `zynq.pl2ps_irq[5]`

Registers are aligned 32-bit, little-endian accesses. Offsets are relative to the physical base:

| Offset | Name | Meaning |
| --- | --- | --- |
| `0x00` | CONTROL | Write 1 to START; write 2 to ACK completion |
| `0x04` | STATUS | BUSY=1, DONE=2, ERROR=4; bits may combine |
| `0x08` | N_ELEMENTS | 0-4096, multiple of 32 |
| `0x0c` | RESULT | FP32 result bits, valid after successful completion |
| `0x10` | ERROR | 0 success; 1 invalid length; 2 nonfinite active scale |
| `0x14` | IRQ_ENABLE | 0 disables; 1 enables completion IRQ |
| `0x18` | ID | `0x51384450` |
| `0x1c` | CYCLES | Estimated cycles for the last accepted valid-length command |
| `0x20` | CAPACITY | 4096 elements (`0x00001000`) |
| `0x1000-0x2fff` | X input | 128 padded slots |
| `0x3000-0x4fff` | Y input | 128 padded slots |

Execution sequence:

1. ACK any previous completion, fill input buffers, and set N_ELEMENTS.
2. Enable the IRQ if desired, then write START.
3. Poll DONE or wait for completion notification. Check ERROR and read RESULT.
4. ACK completion before starting the next command.

Input/configuration writes, START, and ACK are rejected while BUSY. A new START is also rejected until a previous DONE is acknowledged. The IRQ is level-sensitive and remains high while IRQ_ENABLE and DONE are both set. ACK deasserts it. Active-high reset cancels pending work, clears storage/state, and disables interrupts. IRQ output updates occur in a single SC_METHOD to avoid SystemC multiple-writer errors.

Malformed register accesses, byte enables, streaming bursts, and writes to read-only registers return TLM errors. DMI is disabled.

## 6. Timing assumptions

The computation delay is an **estimate** based on achieved II=1 and the HLS maximum latency:

```text
modeled_cycles = n_blocks + 13
modeled_delay = modeled_cycles x 10 ns
```

At 128 blocks this gives `128+13=141 cycles`, matching the HLS maximum and exceeding the observed RTL maximum of 138 cycles. The formula is an inferred conservative estimate, not a measured per-length/per-input latency curve. RTL can finish shorter or zero vectors earlier. The model implements this delay with a SystemC `wait()` in the worker process.

Each valid blocking TLM access also consumes its incoming delay plus an assumed 10 ns bus-service time. This bus delay is a modeling choice, not a measured board parameter. CPU input transfers and synchronization therefore contribute separate simulated time.

## 7. SystemC tests and platform integration completed

On mario:

```bash
cd /misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc
module load systemc/2.3.4
make test
```

Observed output:

```text
MATH PASS: 410 golden vectors (0 ULP), 63488 finite-half scale cases, and invalid-input checks.
SYSTEMC TESTS PASSED: 410 golden vectors plus protocol/timing/reset checks; failures=0
Info: /OSCI/SystemC: Simulation stopped by user.
```

The last line is normal: the test calls `sc_stop()` after completing. SystemC checks cover numerical results, valid/invalid transactions, timing, busy-state protection, completion acknowledgement, interrupt control, reset cancellation, and post-reset operation.

We installed the model and rebuilt the platform:

```bash
python3 install_platform.py /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
cd /misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo
module load systemc/2.3.4
make zynqmp_demo
```

Installation preserved original `Makefile` and `zynqmp_demo.cc` under:

```text
/misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo/lab3_before_dot
```

The platform changes added one TLM target, connected its reset and unused interrupt index 5, mapped the new device, and linked `dot_accel.o` and `dot_math.o`. Existing demo devices and disabled waveform tracing were retained. The build completed successfully. The unused `top` warning remained harmless after tracing was disabled.

## 8. ARM/QEMU validation completed

After restarting the updated QEMU/SystemC pair and logging in as `petalinux`, we probed the device:

```bash
sudo peek 0xa1000018
sudo peek 0xa1000020
```

Observed values:

```text
0x51384450
0x00001000
```

We then cross-compiled `dot_mmio_smoke` on mario and transferred it into QEMU Linux. It uses `/dev/mem`, writes inputs through 32-bit MMIO transactions, and polls completion with the accelerator IRQ disabled.

Observed output:

```text
blocks=0 result=0 expected=0 modeled_cycles=13
blocks=1 result=32 expected=32 modeled_cycles=14
blocks=3 result=96 expected=96 modeled_cycles=16
blocks=128 result=4096 expected=4096 modeled_cycles=141
QEMU MMIO SMOKE TEST PASSED
```

This validates the integrated ARM input-transfer/start/completion/result path, including the full supported vector capacity. The printed cycles are model values, not newly measured RTL latency.

The new accelerator's IRQ passed standalone SystemC tests. Its interrupt delivery through the integrated Linux driver has not been tested yet because that accelerator-specific driver is Part 4 work. The earlier tutorial driver/IRQ test exercised the original debug device, not this new accelerator.

## 9. Resume and reproduction commands

The recorded tests have passed; there is no need to rerun HLS validation merely to resume Part 4. If the simulators are still running, use the existing sessions. Do not start a second pair using the same working directory.

### Start the platform if it has stopped

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

Log in as `petalinux` in Terminal 1. SystemC remaining active without a shell prompt is normal.

### ARM compiler environment

In a new host build terminal, `module load xilinx/2022` alone did not put `aarch64-linux-gnu-gcc` on PATH. The successful setup was:

```bash
module load xilinx/2022
bash
source /usr/local/packages/Xilinx_2022.2/Vivado/2022.2/settings64.sh
command -v aarch64-linux-gnu-gcc
cd /misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc
make arm-smoke
```

If already in Bash, skip starting another Bash shell. The SystemC executable is built with native host `g++`; ARM application binaries use the cross-compiler.

### Transfer/run the MMIO test again if needed

Inside QEMU Linux:

```bash
scp sb69942@mario.ece.utexas.edu:/misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc/dot_mmio_smoke .
sudo ./dot_mmio_smoke
```

SCP uses mario account authentication; guest sudo uses the `petalinux` password. No passwords are recorded here.

### Clean shutdown before pausing

In QEMU Linux, run `sudo poweroff`. Wait for `reboot: Power down`, press Ctrl+A, release both keys, then press x to exit QEMU. Stop the host SystemC process with Ctrl+C if it has not exited. At this handoff, shutdown has been recommended but has not been confirmed in the shared output.

## 10. Files to keep and next session

Keep these working directories and their sources/reports:

- `/misc/scratch/sb69942/soc/Lab3/lab3_alternate_128_validation`
- `/misc/scratch/sb69942/soc/Lab3/lab3_part3_systemc`
- `/misc/scratch/sb69942/soc/Lab3/systemctlm-cosim-demo`
- `/misc/scratch/sb69942/soc/Lab3/images`

Prepared archives:

- `ECE382M_Lab3_Alternate128_Validation.tar.gz`: expanded HLS kernel/tests/scripts.
- `ECE382M_Lab3_Part3_SystemC.tar.gz`: SystemC model/tests/platform integration package. Preserve current mario sources and subsequent test evidence along with it.
- `lab3_platform_inputs.tar.gz`: original platform files supplied before integration.

For final submission, assemble the required sources, platform integration, README/build/run instructions and selected evidence under the lab repository's required `part3/` directory. The report still needs to document this work, its results, and modeling assumptions. Repository upload and Canvas submission have not been confirmed.

**Next session: Part 4(a), the new accelerator's kernel driver and HAL.** Then integrate the HAL into llama.cpp, cross-compile, and validate Tiny-Stories in co-simulation. End-to-end LLM correctness/performance and accelerator-driver IRQ delivery remain unverified. Bus mastering/DMA extra credit has not been implemented.

The immediate next input requested is the tutorial driver and device-tree files. This archive command was suggested, but creation/upload has not yet been confirmed:

```bash
tar -czf /misc/scratch/sb69942/soc/Lab3/lab3_part4_inputs.tar.gz \
  --exclude='*.o' --exclude='*.ko' --exclude='*.cmd' \
  --exclude='example' \
  -C /misc/scratch/sb69942/soc/Lab3/QEMU_SystemC_app \
  application.irq boot
```

Upload `lab3_part4_inputs.tar.gz` at the start of the next session. The new driver must use the new accelerator register protocol and interrupt line; the tutorial driver cannot simply be reused unchanged. We will also inspect the current llama.cpp source and model details when integrating the HAL.

## Sources

This note is based on the supplied Lab 3 handout/tutorial, Lab 2 handoff, uploaded Lab 2 repository archive (including `alternate_optimal_design`), supplied platform files, uploaded expanded-kernel synthesis/co-simulation reports, and the successful mario/SystemC/QEMU outputs shared in this session. Implementation details are in the current Part 3 source package and its README.
