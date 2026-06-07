import { NitroModules } from 'react-native-nitro-modules'
import type {
  ExtractPngChunksOptions,
  PngUtils as PngUtilsSpec,
  ReplacePngChunksOptions,
  TextChunk,
} from './specs/png-utils.nitro'

const PngUtils = NitroModules.createHybridObject<PngUtilsSpec>('PngUtils')

/**
 *
 * @param pngData base64 encoded PNG data
 * @param options extract options
 *  - keywords - filters tEXt chunk keywords, returns all chunks if not specified
 *  - decodeBase64 - whether to run b64 decode on extracted chunks, default true
 * @returns contents of tEXt chunk
 */
export function extractPngTextChunk(
  pngData: string,
  options: ExtractPngChunksOptions = { decodeBase64: true }
) {
  console.log(options)
  return PngUtils.extractPngChunks(pngData, options)
}

/**
 *
 * @param fileData base64 encoded PNG data
 * @param chunks tEXt chunks to insert {keyword, data}
 * @param options replace options
 *  - removeKeywords - deletes tEXt chunks with specified keywords
 * @returns base64 encoded PNG with tEXt chunk replaced
 */
export function replacePngTextChunk(
  fileData: string,
  newData: TextChunk[] | TextChunk,
  options: ReplacePngChunksOptions = {}
) {
  return PngUtils.replacePngChunks(
    fileData,
    Array.isArray(newData) ? newData : [newData],
    options
  )
}
