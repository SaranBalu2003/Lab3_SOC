#!/usr/bin/env python3
"""Install the reviewed platform changes, checking hashes and preserving originals."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', type=Path, help='Existing systemctlm-cosim-demo directory')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    target = args.platform.expanduser().resolve()
    hashes = json.loads((root / 'platform_hashes.json').read_text())
    platform_files = ['zynqmp_demo.cc', 'Makefile']
    model_files = ['dot_accel.h', 'dot_accel.cpp', 'dot_math.h', 'dot_math.cpp', 'dot_regs.h']
    backup = target / 'lab3_before_dot'
    # Finish all checks before modifying any file.
    for name in platform_files:
        dest = target / name
        if not dest.is_file(): raise RuntimeError('Missing platform file: ' + str(dest))
        if digest(dest) not in (hashes[name]['original_sha256'], hashes[name]['installed_sha256']):
            raise RuntimeError('Platform file changed since upload: ' + str(dest) + '. Preserve it and review the patch before installing.')
        if (backup / name).exists() and digest(backup / name) != hashes[name]['original_sha256']:
            raise RuntimeError('An unrelated backup exists: ' + str(backup / name))
        if digest(root / 'platform' / name) != hashes[name]['installed_sha256']:
            raise RuntimeError('Packaged platform file checksum mismatch: ' + name)
    for name in model_files:
        dest = target / name
        if dest.exists() and digest(dest) != digest(root / name):
            raise RuntimeError('Refusing to overwrite a different model file: ' + str(dest))
    backup.mkdir(exist_ok=True)
    for name in platform_files:
        if not (backup / name).exists():
            shutil.copy2(root / 'upstream' / 'platform' / name, backup / name)
    for name in model_files:
        shutil.copy2(root / name, target / name)
    for name in platform_files:
        shutil.copy2(root / 'platform' / name, target / name)
    print('Installed the accelerator and reviewed ZynqMP platform changes into ' + str(target))
    print('Original Makefile and top-level source preserved in ' + str(backup))
    print('Next: make -C ' + str(target) + ' zynqmp_demo')

if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as exc:
        print('Installation stopped: ' + str(exc), file=sys.stderr)
        sys.exit(1)
