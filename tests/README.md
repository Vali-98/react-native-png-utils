# Native checks

These test tools are not included in the published package. Fixtures are generated
test tones, not third-party recordings. No device/emulator execution is automatic.

## PNG

```sh
cmake -S tests/native -B /tmp/png-tests -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/png-tests -j 2
ctest --test-dir /tmp/png-tests --output-on-failure
```

The production implementation is compiled with a minimal Nitro binding substitute.
Checks cover PNG framing, CRC preservation, replacement/filtering, image conversion
and base64 boundaries. On ARM64 the default exercises NEON; repeat in a separate
build with `-DPNGUTIL_NEON=OFF` for scalar coverage. Add
`-DPNGUTIL_SANITIZE=ON -DPNGUTIL_SANITIZERS=undefined` for UBSan. ASan is available
with `-DPNGUTIL_SANITIZERS=address,undefined` if the installed runtime supports it.

`pngutils-benchmark input-MiB iterations extract|replace|convert` is an optional
host benchmark. Synthetic IDAT isolates chunk editing; conversion uses RGB PPM.
RSS includes fixture setup, not just library allocations. `native/compare.py`
compares two built executables; keep generated records and binaries outside Git.

## Audio

On an Apple host:

```sh
cmake -S tests/audio -B /tmp/audio-tests -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/audio-tests -j 2
ctest --test-dir /tmp/audio-tests --output-on-failure
python3 tests/audio/check.py /tmp/audio-tests/audio-convert
```

The Apple backend needs access to the system codec service. The check verifies
PCM format, duration, non-silent output, RIFF lengths and error cleanup. WebM/Opus
is an optional support probe: the current Apple backend rejects it. CMake also
supports cross-compiling the converter with an Android NDK toolchain.

Writer tests cover exclusive creation, incomplete-frame rejection, the WAV size
limit and cleanup. The Android loop regression compiles the actual production
loop against a fake NDK boundary to test negative priming timestamps, EOF flags,
encrypted samples and read errors. It does not replace device codec testing.

`audio/fixtures.json` describes the small binary inputs under `audio/fixtures/`.
The example's `scripts/audio-fixtures.cjs` embeds those files into its generated
fixture module. AAC fixtures use FFmpeg's AAC encoder; MP3 uses libmp3lame and
WebM uses libopus. FFmpeg is a fixture-generation tool, not a runtime dependency.
