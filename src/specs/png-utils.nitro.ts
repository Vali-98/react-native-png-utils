import { type HybridObject } from 'react-native-nitro-modules'

export type TextChunk = {
  keyword: string
  data: string
  /**
   * Encode the output in b64
   */
  b64encode?: boolean
}

export type TextChunkResult = {
  keyword: string
  data: string
}

export type ReplacePngChunksOptions = {
  /**
   * Removes tEXt chunks with specified keywords
   */
  removeKeywords?: string[]
}

export type ExtractPngChunksOptions = {
  /**
   * filters keywords, returns all tEXt chunks if undefined
   */
  keywords?: string[]
  /**
   * Whether to run b64 decode on found tEXt chunks, default true
   */
  decodeBase64?: boolean
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
