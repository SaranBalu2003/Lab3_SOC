# Lab 3 preparation: alternate kernel validation at 128 blocks

This package expands the uploaded Lab 2 `alternate_optimal_design` to a chosen capacity of **128 Q8_0 blocks / 4096 elements per input**. It is the separate validation project used before implementing the Lab 3 SystemC wrapper. The fixed-point arithmetic and pipeline pragmas are preserved.

## Status and scope

- The uploaded, original 8-block setup reports achieved block II=1, a 10 ns target period, and passing RTL co-simulation. Those are upstream results.
- The expanded 128-block kernel subsequently passed Vitis HLS 2022.2 C simulation (27 original + 410 extended cases), synthesis (block II=1, iteration latency 4, maximum function latency 141 cycles at a 10 ns target), and Verilog C/RTL co-simulation. The supplied synthesis/co-simulation transcript is preserved in [`../upstream/kernel/validated_reports.txt`](../upstream/kernel/validated_reports.txt).
- Local checks passed for all 410 generated input fixtures against an independent Python exact-arithmetic oracle, the C++ testbench syntax, shell syntax, and Tcl script completeness. These checks are not HLS/RTL validation.
- The parent `part3/` directory contains the completed SystemC integration and tests. Accelerator-specific ARM HAL/driver and llama.cpp changes remain Part 4 work.

## What changed

1. Added `DOT_MAX_BLOCKS=128` to `vec_dot_q8.h`.
2. Changed both input-interface depths from 8 to 128 in `vec_dot_q8.cpp`.
3. Added a capacity assertion. Valid calls require `0 <= n <= 4096`, `n % 32 == 0`, `nrc == 1`, and finite FP16 scales. Stride arguments retain the original scalar behavior.
4. Expanded the original testbench's backing arrays to 128 entries; its original 27 cases still exercise their original lengths and data. The larger backing arrays are needed because the RTL co-simulation interface now declares depth=128.
5. Added 410 tests: 60 directed cases and 350 deterministic random cases. **Total: 437 calls per full testbench run.**

`upstream/` preserves the original sources, testbench, scripts, and selected reports byte-for-byte. `CHANGES.diff` records source/testbench changes against those copies. No original mario or Lab 2 files need to be overwritten.

## Run on mario

This repository stores the project in `part3/hls_validation/`. The commands below record the tested mario working-copy path; substitute the checkout path to reproduce the run.

In a host terminal on mario (not the QEMU Linux shell):

```bash
module load xilinx/2022
bash
cd /misc/scratch/sb69942/soc/Lab3/lab3_alternate_128_validation
bash run_validation.sh csim
```

If already using Bash, do not enter another Bash unnecessarily. The Xilinx environment must make `vitis_hls` available; the simulation tutorial's ARM compiler environment alone does not substitute for Vitis HLS.

The first run does C simulation only. Expect the original 27-case summary followed by:

```text
Extended tests: 410/410 passed
ALL VALIDATION TESTS PASSED: original 27 tests plus 410 extended tests
```

After reviewing a successful C simulation, run the full flow:

```bash
bash run_validation.sh all
```

This intentionally reruns C simulation, then performs synthesis and Verilog/XSim C/RTL co-simulation. Waveform tracing is disabled. It uses a new project, `hls_alt128_validation/solution1`, at the teammate's original 10 ns clock target and FPGA part `xczu3eg-sbva484-1-e`.

Check:

- C simulation passes all 437 cases.
- The block loop (source loop beginning at `for (int ib = 0; ...)`) still achieves II=1. The source line number changes with the capacity assertion, so use the loop content rather than assuming the old `166` identifier.
- HLS timing remains compatible with the 10 ns target including uncertainty.
- RTL co-simulation reports Verilog `Pass`, and the extended testbench completes successfully.

## Output files

| Path | Contents |
| --- | --- |
| `results/validation_csim.log` | First C-simulation-only run log |
| `results/validation_all.log` | Full C simulation, synthesis, and RTL co-simulation log |
| `results/extended_csim_results.csv` | Per-case C-simulation results for the 410 added cases |
| `results/csynth_summary.rpt` | Overall synthesis summary, including achieved loop II |
| `results/alternate128_csynth.rpt` | Top-level synthesis timing, resources, and interface |
| `results/*Pipeline*csynth.rpt` | Individual pipeline reports |
| `results/alternate128_cosim.rpt` | RTL pass/fail and observed transaction timings |
| `results/extended_cosim_results.csv` | Copied RTL post-check CSV when present at the tool's expected location |

If the RTL CSV is not copied automatically, locate the generated file with:

```bash
find hls_alt128_validation/solution1/sim -name extended_results.csv
```

The two summaries and the full log are the primary RTL evidence; a missing optional CSV copy is not itself an RTL failure.

## Numerical validation

The kernel decodes finite half scales into significands/exponents, uses exact fixed-point products and accumulation, then converts the accumulated result to float. This changes the arithmetic relative to the original sequential FP32 implementation.

The Python standard-library generator independently computes sums using `fractions.Fraction`, expressed as dequantized element products. It rounds the exact rational sum to nearest-even binary32 to produce the expected bits. The new testbench allows at most **one ULP** from this final-conversion oracle and requires exact zero for exact-zero cases. This tolerance accommodates a possible final-conversion rounding difference without assuming that the vendor conversion is nearest-even. It is much tighter than a blanket 10% test. Original test assertions are retained.

The exact and sequential-FP32 references differ in 164 of the 410 generated cases. This is expected from the changed arithmetic. Both reference bit patterns are recorded in the generated header and per-case CSV. Difference from sequential FP32 alone is not a failure. Later llama.cpp validation must account for these differences and check application correctness.

Added cases include:

- Empty vectors and 1, 2, 3, 8, 32, 64, 127, and 128 blocks.
- Maximum positive and negative contributions and minimum-subnormal scales.
- Last-element access and ignoring populated data beyond the requested length.
- Cancellation that preserves a residual of 1 or 2^-48.
- 350 deterministic random inputs using signed finite FP16 scales across the finite encoding range, at seven lengths through 128 blocks.

Generated inputs always initialize the full 128-block backing arrays. Invalid lengths and NaN/Inf scales are outside the valid-input contract; the kernel asserts on these rather than treating them as supported test cases.

## Rebuild or inspect the reference data locally

The generated `golden_cases.h` is included; Python is not needed for the Vitis run.

To regenerate and check the reference data on a host with Python 3 and GCC:

```bash
python3 generate_cases.py
g++ -std=c++11 -O2 -ffp-contract=off check_oracle.cpp -o check_oracle
./check_oracle
```

`check_oracle` verifies the shared C++ fixture generator against the Python exact and sequential-FP32 references. It does not build or run the HLS kernel. `oracle_summary.json` records the test counts, not a Vitis pass.

## Integration implications

- II=1 refers to block launches inside a vector. The whole function is not a pipeline accepting one new vector every cycle.
- The original 512-bit ports require suitably organized local storage. Packed data for both 128-block inputs is 8704 bytes; reserving one 512-bit word per block per input would use 16384 bytes. These are alternative storage representations, not additive capacities.
- The original zero internal-BRAM report excludes external input memories. The future SystemC platform must model those buffers and their access timing.
- Do not reuse the original 8-block maximum latency of 18 cycles as the latency for a 128-block call. Use the new reports and per-length evidence to construct the SystemC delay model.
- Reducing II does not establish an end-to-end llama.cpp speedup; CPU transfers, synchronization, and buffer reuse must also be measured in Lab 3.

## Next step

The expanded C/RTL validation and the SystemC model in the parent directory are complete. Continue with the accelerator-specific driver/HAL and llama.cpp integration described in [`../TEAM_HANDOFF.md`](../TEAM_HANDOFF.md). The original `ap_fixed` and `half` types are vendor dependencies; any portable SystemC functional model must preserve the selected fixed-point arithmetic rather than silently reverting to sequential float accumulation.

The original licenses are retained as `LICENSE` and `LICENSE.fp16`. The original source archive and checksum are recorded in `PROVENANCE.txt`.
