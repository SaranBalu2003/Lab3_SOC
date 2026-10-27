# Local checks performed before delivery

- Generated 410 added cases using an independent exact Fraction oracle: passed.
- Compiled and ran the native C++ fixture/reference checker: all 410 matched both Python reference bit patterns.
- Exact accumulation versus sequential-FP32 reference differences: 164/410.
- Checked the expanded testbench's C++11 syntax using a temporary host `_Float16` declaration shim: passed. This was a syntax check only, not a substitute for the vendor half type or the HLS kernel.
- Checked `run_validation.sh` with `bash -n`: passed.
- Checked Tcl script syntactic completeness with a Tcl interpreter: passed.
- Compared preserved upstream files with the uploaded archive: passed during packaging.

These were the checks performed locally before the original delivery; Vitis HLS was unavailable in that preparation environment. Subsequent runs on mario passed C simulation, synthesis with achieved block II=1, and Verilog C/RTL co-simulation. See `../TEAM_HANDOFF.md` and `../upstream/kernel/validated_reports.txt` for the completed validation record.
