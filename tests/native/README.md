Native regression tests compile the production C++ implementation with a minimal
Nitro binding substitute. They do not require a running React Native app. The
Android release build separately verifies the real generated bindings.

From the repository root:

```sh
cmake -S tests/native -B /tmp/png-utils-tests -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/png-utils-tests -j 2
ctest --test-dir /tmp/png-utils-tests --output-on-failure
```

On ARM64 hosts, the default build exercises NEON. Repeat with
`-DPNGUTIL_NEON=OFF` in a separate build directory to exercise scalar decoding.

For sanitizers, add `-DPNGUTIL_SANITIZE=ON`. If the local ASan runtime is
incompatible with the OS, `-DPNGUTIL_SANITIZERS=undefined` runs UBSan alone.
Sanitizer findings terminate the test immediately. On macOS, an additional
Guard Malloc run is available:

```sh
env DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib MallocNanoZone=0 \
  /tmp/png-utils-tests/pngutils-tests
```

Tests cover base64 lengths 0–1024 against an independent reference encoder,
explicit `b64encode: false`, invalid characters throughout SIMD/scalar blocks,
truncated images, chunk bounds, filtering, replacement/removal, CRC preservation,
and non-PNG conversion with decoded pixel verification.

The benchmark accepts `input-MiB iterations extract|replace|convert`. PNG cases
use synthetic IDAT data of the requested size: they measure container editing,
not pixel decoding. Conversion uses a deterministic 1024×1024 RGB PPM.
`maxrss` is whole-process high-water RSS (bytes on macOS, KiB on Linux), including
fixture construction; it is not isolated library allocation or JS heap usage.

To compare implementations, configure a second build with `PNGUTIL_SOURCE` and
`PNGUTIL_BASE64_DIR` pointing to the original sources, and disable
`PNGUTIL_NEON` and `PNGUTIL_BASE64_CHECK` there to reproduce the original path.
Run only its `pngutils-benchmark` target; current regression tests intentionally
reject some original behavior.

```sh
python3 tests/native/compare.py /tmp/original/pngutils-benchmark \
  /tmp/png-utils-tests/pngutils-benchmark --output /tmp/png-comparison.json
```

See [the measured results](../../docs/performance.md) for build sizes and limits
of the host measurements.
