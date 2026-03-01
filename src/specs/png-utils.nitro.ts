import { type HybridObject } from 'react-native-nitro-modules'

export interface PngUtils extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  extractPngChunk(pngData: string, decodeOutput: boolean): string
  replacePngChunk(
    pngData: string,
    newData: string,
    encodeInput: boolean
  ): string
}
