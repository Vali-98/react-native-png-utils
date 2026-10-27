#!/usr/bin/env python3
"""Validate a host/native converter executable against generated audio fixtures."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('converter', type=Path)
args = parser.parse_args()
fixtures = Path(__file__).resolve().parent / 'fixtures'
with tempfile.TemporaryDirectory(prefix='png-audio-check-') as directory:
    root = Path(directory)
    cases = json.loads((fixtures.parent / 'fixtures.json').read_text())
    for case in cases:
        filename = case['filename']
        rate, channels, seconds = case['sampleRate'], case['channels'], case['durationSeconds']
        output = root / (filename + '.wav')
        result = subprocess.run([str(args.converter.resolve()), str(fixtures / filename), str(output)], capture_output=True, text=True)
        if result.returncode != 0 and sys.platform == 'darwin' and not case['appleSupported']:
            assert not output.exists()
            print(filename, 'unsupported by Apple AudioToolbox (clean rejection)')
            continue
        assert result.returncode == 0, result.stderr
        data = output.read_bytes()
        assert struct.unpack_from('<I', data, 4)[0] == len(data) - 8
        assert struct.unpack_from('<I', data, 40)[0] == len(data) - 44
        with wave.open(str(output)) as wav:
            assert wav.getframerate() == rate and wav.getnchannels() == channels
            assert wav.getsampwidth() == 2
            assert abs(wav.getnframes() / rate - seconds) < 0.15
            pcm = wav.readframes(wav.getnframes())
            assert any(pcm), 'Expected a non-silent test tone'
        previous = hashlib.sha256(data).digest()
        duplicate = subprocess.run([str(args.converter.resolve()), str(fixtures / filename), str(output)], capture_output=True)
        assert duplicate.returncode != 0
        assert hashlib.sha256(output.read_bytes()).digest() == previous, 'Existing output was modified'
        print(filename, 'PCM format, duration, non-silent samples, RIFF lengths and no-overwrite passed')
    bad = root / 'invalid.media'
    bad.write_bytes(b'This is not media')
    for source in [bad, root / 'missing.m4a']:
        output = root / 'failure.wav'
        result = subprocess.run([str(args.converter.resolve()), str(source), str(output)], capture_output=True)
        assert result.returncode != 0 and not output.exists(), source
    assert not list(root.glob('*.tmp-*')), 'Temporary WAV leaked'
    print('Missing and corrupt media input rejection passed')
