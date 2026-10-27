# @vali98/react-native-png-utils

Native PNG text-chunk utilities and file-to-file audio conversion for React
Native, using Nitro Modules. Android and iOS implementations share C++ interfaces.

## Installation

```sh
npm install @vali98/react-native-png-utils react-native-nitro-modules
```

Rebuild the native app after installation. On iOS, run `pod install`.

## PNG metadata

```ts
import {
  extractPngTextChunk,
  replacePngTextChunk,
} from '@vali98/react-native-png-utils'

const chunks = extractPngTextChunk(pngBase64, {
  keywords: ['chara'],
  decodeBase64: true,
})

const updated = replacePngTextChunk(pngBase64, [
  { keyword: 'chara', data: JSON.stringify(card), b64encode: true },
], { removeKeywords: ['obsolete'] })
```

Extraction returns `{ keyword, data }[]`. Without a keyword filter it reads all
`tEXt` chunks; base64 values are decoded by default when valid. Replacement removes
existing chunks with matching keywords and inserts the supplied values. PNG pixel
data is retained without decoding or recompression. Non-PNG image replacement
uses STB to decode and convert the image to PNG. These methods are synchronous.

## Audio to WAV

```ts
import { convertToWav } from '@vali98/react-native-png-utils'

const result = await convertToWav(inputFileUri, outputFileUri)
// { path, uri, sampleRate, channels, frames, durationSeconds, bytes }
```

Both arguments must be caller-provided local `file://` URIs. The library does not
choose a cache path. Copy `content://` documents into app-local storage before
conversion. The output directory must exist and the output file must not exist.
`result.uri` is the supplied destination; `result.path` is its decoded filesystem
path. No external-storage permissions are needed for app-private cache files.

There is **no file-extension allowlist** in the converter. It asks Android's NDK
media APIs or Apple's AudioToolbox to decode the first/default audio track. MP3,
M4A/AAC and audio in MP4 are the primary use cases. Other formats are attempted
using the same path; support depends on the OS decoder and container reader.
The current Apple AudioToolbox backend does not accept the included WebM/Opus
fixture. WebM support is therefore not guaranteed across platforms. WebP is an
image format and contains no audio.

Output is signed little-endian 16-bit PCM WAV, preserving the decoded sample rate
and mono/stereo channel count. Multichannel audio, encrypted media, empty audio,
midstream format changes and classic WAV output over 4 GiB are rejected. Conversion
does not restore lossy source information or preserve 24/32-bit precision. There
is no resampling, downmixing, track-selection or progress/cancellation option.

Conversion runs on Nitro's worker pool with bounded PCM buffers. It creates the
requested file exclusively and removes incomplete output on failure. Wait for the
promise before reading the WAV. Disk space still scales with duration: 48 kHz
stereo 16-bit PCM uses about 11.5 MB per minute.

## Example and development

The [example app](example/README.md) includes PNG benchmarks and an Audio → WAV
screen with local file selection and generated test tones. Its audio output is
`result.wav` in app cache; only the example chooses that path.

```sh
npm run build
```

After changing Nitro specs, run `npx nitrogen` and include its generated bindings
in the change. Avoid the full `codegen` script unless you also intend to rebuild
JS and install example iOS pods. See [test instructions](tests/README.md).

The package publishes source and native bindings, not example fixtures, tests,
benchmark binaries or build directories.

## Credits

Bootstrapped with create-nitro-module. PNG conversion uses STB; base64 conversion
uses the bundled Turbo-Base64 implementation with ARM64 NEON selection.
