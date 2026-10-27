import type { HybridObject } from 'react-native-nitro-modules'

export type WavConversionResult = {
  path: string
  sampleRate: number
  channels: number
  frames: number
  durationSeconds: number
  bytes: number
}

export interface AudioUtils
  extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  /** Local absolute filesystem paths. Creates a new 16-bit PCM WAV; never overwrites. */
  convertToWav(
    inputPath: string,
    outputPath: string
  ): Promise<WavConversionResult>
}
