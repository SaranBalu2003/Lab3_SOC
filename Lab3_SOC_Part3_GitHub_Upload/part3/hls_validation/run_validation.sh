#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
stage="${1:-all}"
case "$stage" in csim|all) ;; *) echo "Usage: bash run_validation.sh [csim|all]" >&2; exit 2 ;; esac
command -v vitis_hls >/dev/null || { echo 'Load the Vitis HLS 2022.2 environment first.' >&2; exit 1; }
export LAB3_VALIDATE_STAGE="$stage"
mkdir -p results
vitis_hls -f run_validation.tcl 2>&1 | tee "results/validation_${stage}.log"
