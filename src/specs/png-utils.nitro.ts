import { type HybridObject } from 'react-native-nitro-modules'

export interface PngUtils extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  getPngChunk(pngData: string): string
}
