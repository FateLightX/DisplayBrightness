from pathlib import Path
root = Path(__file__).resolve().parents[1]
for path in (root / 'src').rglob('*'):
    if path.is_file():
        content = path.read_text()
        for forbidden in ('hidpi', 'scale-resolutions', 'setDisplayAttributes', 'isPilotDell', 'AMDRadeonX6000_Amd', 'DisplayBrightnessAMDMetadata', '-dbprobe', '-dbuiprobe', '-dbedmprobe', '-dbhwprobe', '-dbddcprobe', '-dbagdcmeta'):
            assert forbidden.lower() not in content.lower(), f'{path.name}: removed feature remains: {forbidden}'
print('brightness-only source: no resolution injection or display-attribute route')
