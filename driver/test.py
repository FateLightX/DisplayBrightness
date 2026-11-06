#!/usr/bin/env python3
"""Run the portable policy tests and check the kernel-side startup wiring."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent
compiler = subprocess.check_output(['xcrun', '--find', 'clang++'], text=True).strip()
tests = sorted((root / 'tests').glob('*-test.cpp'))
with tempfile.TemporaryDirectory(prefix='displaybrightness-tests-') as temp:
    for source in tests:
        binary = Path(temp) / source.stem
        subprocess.run([compiler, '-std=c++14', '-Wall', '-Wextra', '-Werror',
                        '-I', str(root / 'src'), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
subprocess.run(['python3', str(root / 'tests' / 'check-brightness-only.py')], check=True)
subprocess.run(['python3', str(root / 'tests' / 'check-startup-wiring.py')], check=True)
print(f'PASS: {len(tests)} C++ tests and 2 source checks (no live kernel I/O)')
