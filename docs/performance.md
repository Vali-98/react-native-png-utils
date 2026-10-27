# PNG utilities performance

Measured on 2026-10-27 against commit
`39d9abf08acbd01e7f1b34c47e7147c3cc5f989a` (0.2.2).

## RAM and execution time

The primary changes reduce runtime allocations and image-sized copies:

- Parsing uses views into one decoded input buffer. Extraction no longer copies
  IDAT or other unused payloads into an owning chunk list.
- Replacement writes retained chunks directly into one exactly reserved output
  buffer. The parsed/before-IDAT/after-IDAT/combined payload collections are gone.
- Unchanged chunks retain their original bytes and CRCs. Only inserted metadata
  needs a new CRC.
- Base64 output is written directly into its final string. There is no temporary
  image-sized base64 vector. The binary input buffer is freed before this final
  string is allocated.
- Metadata keys and values are inspected as views before allocating results.
- Non-PNG conversion uses the STB pixel allocation directly, avoiding a second
  full RGBA buffer, and releases the compressed source before encoding. Dimension
  limits are checked before pixel decoding. The encoder reuses platform zlib.
- ARM64 selects NEON at compile time; other Android ABIs use scalar base64.
  There is no runtime CPU dispatcher or bundle of unrelated SIMD implementations.

The API still accepts/returns whole base64 strings and executes synchronously.
It therefore still needs image-sized buffers and can block the caller. This
change does not eliminate React Native's JS/native string copies or measure their
RAM cost. Existing PNGs are never decoded to pixels.

Host measurements below use the production C++ implementation with a minimal
binding substitute on an ARM64 Mac, AppleClang 17, CMake Release (`-O3`). Each
entry is the median of three separate processes, alternating before/after order.
The final runs had similar wall and process CPU times. These are **host native
measurements, not Android device or complete React Native app measurements**.

| Operation | Input | CPU ms/call before | CPU ms/call after | Speedup | Peak process RAM before | Peak process RAM after |
|---|---|---:|---:|---:|---:|---:|
| Extract | 8 MiB PNG | 1.552 | 0.688 | 2.26× | 36.2 MiB | 36.2 MiB |
| Replace | 8 MiB PNG | 3.844 | 1.374 | 2.80× | 76.3 MiB | 49.5 MiB |
| Extract | 32 MiB PNG | 6.200 | 2.895 | 2.14× | 140.2 MiB | 140.2 MiB |
| Replace | 32 MiB PNG | 15.961 | 5.858 | 2.72× | 300.3 MiB | 193.5 MiB |
| Convert + replace | 1024×1024 RGB PPM | 137.186 | 112.158 | 1.22× | 60.9 MiB | 29.9 MiB |

RAM figures are median process high-water RSS, including fixture construction and
allocator retention. Fixture construction dominates extraction's high-water mark,
so those rows do not isolate its removed image-sized allocation. Replacement
reduced this host process peak by about **35%** for both PNG sizes; conversion
reduced it by about **51%**. Device RAM savings need validation in a consuming app.
The conversion fixture's base64 output shrank from 5,594,772 to 4,875,236 bytes
(12.9%); it is one deterministic image, not a compression-quality survey.

PNG benchmark IDAT payloads are synthetic data of the requested size. They
exercise chunk editing without pixel decoding. Extraction reads one small text
chunk; replacement substitutes one small base64-encoded text chunk. The
conversion benchmark includes real image decoding and PNG encoding.

Raw measurements: [benchmark records](performance-benchmarks.json).
Reproduction and fixture details: [native test README](../tests/native/README.md).

## Built Android library sizes

Both revisions were built using the example app's actual generated Nitro
bindings, Gradle `externalNativeBuildRelease`, React Native 0.82.1, Nitro 0.31.2,
NDK 27.1.12297006, and the same effective `-O3` optimization settings. These are
actual `libPngUtils.so` ELF file sizes after NDK `llvm-strip --strip-unneeded`,
not object-file estimates or APK download sizes. Shared dependencies are excluded
from both sides. The optimized ARM64 disassembly contains calls to both
`tb64v128enc` and `tb64v128dec`; `JNI_OnLoad` remains exported on every ABI.

| ABI | Before, bytes | After, bytes | Reduction |
|---|---:|---:|---:|
| arm64-v8a | 327,000 | 254,624 | 22.1% |
| armeabi-v7a | 211,244 | 159,544 | 24.5% |
| x86 | 350,528 | 259,348 | 26.0% |
| x86_64 | 345,952 | 260,848 | 24.6% |

Removing unused STB entry points and hiding internal base64 C symbols offsets
SIMD's code cost. Debug-bearing, unstripped build outputs are approximately
2.1–2.4 MB; they are not representative of shipped stripped code.

The local stripped artifacts are under
`android/build/performance/{baseline,optimized}/<ABI>/libPngUtils.so`.
[Size records and optimized SHA-256 hashes](performance-sizes.json) identify the
measured artifacts. Build directories are intentionally ignored by Git.

```sh
cd example/android
./gradlew :vali98_react-native-png-utils:externalNativeBuildRelease --offline --console=plain
```

## Validation and behavior

- Android release native builds pass for all four ABIs with the real bindings.
- Native regression tests pass in Release and under UBSan for both ARM64 NEON
  and scalar paths; sanitizer reports terminate the test immediately.
- The same regression suite passes with macOS Guard Malloc. Its default
  16-byte allocation alignment can miss small overreads; it is not an ASan pass.
- The installed Apple ASan runtime deadlocks while initializing shadow memory,
  before `main`. A process sample confirmed the runtime initialization recursion;
  those two stalled test processes were terminated. ASan validation remains
  unavailable on this host.
- TypeScript typecheck and Ruby podspec syntax checks pass. An iOS app build and
  Android device execution were not performed.

The public signatures are unchanged. Explicit `b64encode: false` now means false,
and encoding an empty metadata value succeeds. Complete base64 validation and
safe unaligned scalar loads replace the bundled decoder's partial checks; signed
shifts in its encoder tail were also corrected. SIMD receives only complete
blocks, with padding handled by scalar tails to avoid input overreads and padding
rejection at block boundaries.

Malformed PNG framing (missing header/end, oversized or truncated chunks) now
throws instead of being accepted or silently rebuilt. New keywords must have
1–79 bytes without NUL. As before, this is not a full PNG validator: pixel data,
chunk ordering beyond header/end framing, and input CRCs are not validated.
Unlike the previous writer, unchanged chunks keep their original CRCs, including
an existing invalid CRC. A PNG replacement that changes nothing returns the
original base64 string after framing validation.
