# Example app

Install the workspace dependencies, then run from this directory:

```sh
npm run fixtures
npm start
# In another terminal:
npm run android
# or npm run ios
```

The start/android/ios lifecycle scripts regenerate `audioFixtures.ts` from the
small native test inputs. That generated base64 embedding is ignored by Git.
No media files are downloaded. Rebuild the app for native changes; iOS also needs
`pod install` after adding Nitro objects or native files.

## Audio → WAV

Pick any local file; the picker does not filter extensions or MIME types. The
file is copied to app cache and passed as a local file URI. The native decoder
determines whether it contains supported audio. Generated fixtures cover AAC
M4A, MP3, MP4 and WebM/Opus; the current Apple backend rejects WebM/Opus.

Conversion runs on a worker and shows elapsed time, format, duration and the
output URI. The screen owns `result.wav` in app cache and replaces it for each
conversion. Use the clear button to remove its current cached input and output.

## PNG benchmarks

Use Manual images to load a PNG, or Random PNG to generate a valid random-pixel
PNG at 1, 2, 3, 8 or 16 MiB. Generation and file reading are outside measured calls.
Results show median, p95 and mean for extraction/replacement after one warm-up.
Use a release build for comparisons. Calls are synchronous with yields between
iterations; timings include the JS/native boundary.

Only eight results are retained. Extracted text previews are selectable and
limited to 16,384 characters across 32 chunks per result. Replacement images are
not retained. Displayed RAM estimates describe buffer content; measure real
process peaks with the native platform profiler.
