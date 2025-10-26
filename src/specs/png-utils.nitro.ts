import { type HybridObject } from 'react-native-nitro-modules'

export interface PngUtils extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  extractPngChunk(pngData: string): string
  replacePngChunk(pngData: string, newData: string): string
}
