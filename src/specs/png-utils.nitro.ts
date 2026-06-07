import { type HybridObject } from 'react-native-nitro-modules'

export type TextChunk = {
  keyword: string
  data: string
  b64encode?: boolean
}

export type TextChunkResult = {
  keyword: string
  data: string
}

export type ReplacePngChunksOptions = {
  removeKeywords?: string[]
}

export type ExtractPngChunksOptions = {
  keywords?: string[]
  decodeBase64?: boolean // default true
}

export interface PngUtils extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  replacePngChunks(
    imageBase64: string,
    chunks: TextChunk[],
    options?: ReplacePngChunksOptions
  ): string

  extractPngChunks(
    imageBase64: string,
    options?: ExtractPngChunksOptions
  ): TextChunkResult[]
}
