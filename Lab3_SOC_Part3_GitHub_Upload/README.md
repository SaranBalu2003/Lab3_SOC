# ECE382M Lab 3: SystemC/QEMU accelerator

Part 3 implements the team's alternate Lab 2 Q8_0 dot-product design as a timed SystemC/TLM model, integrated with the QEMU ZynqMP platform. The selected HLS block loop achieves II=1 with a 4-cycle iteration latency; blocks overlap. The model supports up to 128 blocks (4096 elements) per input vector.

Start with [part3/TEAM_HANDOFF.md](part3/TEAM_HANDOFF.md), then [part3/README.md](part3/README.md) for implementation and build details.

## Repository contents

| Path | Contents |
| --- | --- |
| `part3/dot_math.*` | Portable exact dot-product arithmetic and final FP32 conversion |
| `part3/dot_accel.*`, `part3/dot_regs.h` | SystemC worker, TLM interface, buffers, control, timing, reset, and IRQ |
| `part3/test_*.cpp`, fixture headers | Numerical and SystemC protocol tests |
| `part3/mmio_smoke.c` | ARM Linux direct-MMIO test using `/dev/mem` |
| `part3/platform/`, installer, patch, hashes | Reviewed integration into the existing `systemctlm-cosim-demo` platform |
| `part3/hls_validation/` | Expanded HLS source, tests, oracle, and reproduction scripts |
| `part3/upstream/` | Preserved platform/kernel sources and supplied validation evidence |

## Recorded validation

- HLS C simulation: 27 original and 410 extended cases passed; extended cases had 0 ULP error against the exact-sum oracle.
- Expanded HLS synthesis: block II=1, iteration latency 4, maximum function latency 141 cycles at a 10 ns target; Verilog C/RTL co-simulation passed.
- Native math tests: 410 golden vectors and 63,488 finite-half scale cases passed.
- Standalone SystemC: 410 golden vectors plus protocol, timing, reset, and interrupt checks passed with zero failures.
- Integrated ARM/QEMU: ID/capacity probes and direct-MMIO computations at 0, 1, 3, and 128 blocks passed.

The computation delay is an estimate: `(blocks + 13) * 10 ns`, with a separate assumed 10 ns TLM access service time. It is not a measured per-input RTL latency curve. The core accumulates exactly before FP32 conversion, so its results can differ from sequential FP32 accumulation.

## Build and use

On mario, from the checkout:

```bash
module load systemc/2.3.4
cd part3
make test
python3 install_platform.py /absolute/path/to/systemctlm-cosim-demo
make -C /absolute/path/to/systemctlm-cosim-demo zynqmp_demo
```

The installer expects the platform version preserved in `part3/upstream/platform/` or this package's already-installed version. It checks hashes before changing files and backs up original platform files. Review `platform_changes.patch` if your platform differs. The external platform repository and its submodules, SystemC, QEMU boot images, and ARM toolchain are prerequisites; this repository contains the integration sources rather than those dependencies.

The handoff records Saran's tested working paths under `/misc/scratch/sb69942/soc/Lab3`. Substitute your own paths when using another account or checkout. The physical group board is not required for this co-simulation.

## Part 4 starting point

Part 4 has not been implemented. Add the accelerator-specific Linux driver/device-tree configuration and HAL, integrate with llama.cpp, and validate TinyStories in co-simulation. The existing smoke test uses polling; delivery of this accelerator's interrupt through Linux remains unverified.

Key interface details:

- Base `0xa1000000`, mapping size `0x10000`.
- X/Y buffers at offsets `0x1000` / `0x3000`.
- Each block uses a 64-byte slot: little-endian FP16 scale in bytes 0-1, signed int8 values in bytes 2-33, then padding.
- Write element count, START, wait for DONE, check ERROR/read RESULT, then ACK before the next command.
- IRQ connects to `zynq.pl2ps_irq[5]`; this index is not the Linux IRQ number.

Use the tutorial `QEMU_SystemC_app/application.irq` and `boot` files from the existing mario setup as driver/device-tree references; they are not included here. The new accelerator requires its own register protocol and interrupt mapping. Models, credentials, guest disk images, and generated build products are not part of this source handoff.
