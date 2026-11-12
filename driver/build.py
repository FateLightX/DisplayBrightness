#!/usr/bin/env python3
"""Build an x86_64 OpenCore kext; does not install or load it."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lilu', type=Path, required=True, help='Lilu DEBUG kext with Contents/Resources headers')
parser.add_argument('--sdk', type=Path, required=True, help='MacKernelSDK directory')
args = parser.parse_args()
root = Path(__file__).resolve().parent
lilu = args.lilu.resolve()
sdk = args.sdk.resolve()
resources = lilu / 'Contents' / 'Resources'
for required in (resources / 'Library' / 'plugin_start.cpp', sdk / 'Headers' / 'IOKit' / 'IOService.h',
                 sdk / 'Library' / 'x86_64' / 'libkmod.a'):
    if not required.is_file():
        parser.error(f'Missing build dependency: {required}')
info = plistlib.loads((root / 'Info.plist').read_bytes())
version = info['CFBundleVersion']
assert version == info['CFBundleShortVersionString']
output = root.parent / 'build' / version
output.mkdir(parents=True, exist_ok=True)
compiler = subprocess.check_output(['xcrun', '--find', 'clang++'], text=True).strip()
linker = subprocess.check_output(['xcrun', '--find', 'ld'], text=True).strip()
flags = ['-arch', 'x86_64', '-std=c++14', '-mkernel', '-fapple-kext', '-fno-exceptions',
         '-fno-rtti', '-fno-stack-protector', '-fno-common', '-fno-builtin',
         '-fno-asynchronous-unwind-tables', '-mno-red-zone', '-mno-mmx', '-mno-sse',
         '-mno-sse2', '-msoft-float', '-fvisibility=hidden', '-nostdinc', '-Os', '-g',
         '-DKERNEL', '-DKERNEL_PRIVATE', '-DDRIVER_PRIVATE', '-DDEBUG=1',
         '-DPRODUCT_NAME=DisplayBrightness', f'-DMODULE_VERSION={version}',
         '-I', str(resources), '-I', str(sdk / 'Headers'), '-I', str(root / 'src')]
with tempfile.TemporaryDirectory(prefix='stage-', dir=output) as temp:
    stage = Path(temp)
    objects = []
    for source in (root / 'src' / 'Plugin.cpp', root / 'src' / 'Kmod.cpp', resources / 'Library' / 'plugin_start.cpp'):
        obj = stage / (source.stem + '.o')
        subprocess.run([compiler, *flags, '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    bundle = stage / 'DisplayBrightness.kext'
    executable = bundle / 'Contents' / 'MacOS' / 'DisplayBrightness'
    executable.parent.mkdir(parents=True)
    shutil.copy2(root / 'Info.plist', bundle / 'Contents' / 'Info.plist')
    subprocess.run([linker, '-arch', 'x86_64', '-kext', '-static', '-o', str(executable),
                    *objects, '-L', str(sdk / 'Library' / 'x86_64'), '-lkmod'], check=True)
    subprocess.run(['xcrun', 'dsymutil', str(executable), '-o', str(stage / 'DisplayBrightness.kext.dSYM')], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(bundle)], check=True)
    subprocess.run(['codesign', '--verify', '--strict', str(bundle)], check=True)
    subprocess.run(['plutil', '-lint', str(bundle / 'Contents' / 'Info.plist')], check=True)
    for name in ('DisplayBrightness.kext', 'DisplayBrightness.kext.dSYM'):
        target = output / name
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(stage / name), target)
archive = output / f'DisplayBrightness-{version}-DEBUG.zip'
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for name in ('DisplayBrightness.kext', 'DisplayBrightness.kext.dSYM'):
        for path in sorted((output / name).rglob('*')):
            if path.is_file():
                z.write(path, path.relative_to(output))
    z.write(root / 'ExportBootLog.command', 'ExportBootLog.command')
    for name in ('README.md', 'README.zh-CN.md', 'CHANGELOG.md', 'LICENSE-Lilu.txt',
                 'docs/DEVELOPMENT.md', 'docs/TROUBLESHOOTING.md'):
        z.write(root.parent / name, name)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
manifest = {
    'version': version, 'architecture': 'x86_64', 'lilu_headers': str(resources),
    'sdk': str(sdk), 'compiler': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
    'source_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in sorted((root / 'src').glob('*')) if p.is_file()},
    'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
    'live_boot_tested': False,
}
(output / 'build-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Built: {archive}\nSHA256: {manifest["archive_sha256"]}\nNot installed or loaded.')
