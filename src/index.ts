import { NitroModules } from 'react-native-nitro-modules'
import type { PngUtils as PngUtilsSpec } from './specs/png-utils.nitro'

const PngUtils = NitroModules.createHybridObject<PngUtilsSpec>('PngUtils')

/**
 *
 * @param pngData base64 encoded PNG data
 * @returns contents of tEXt chunk
 */
export function extractPngTextChunk(
  pngData: string,
  decodeOutput: boolean = true
) {
  return PngUtils.extractPngChunk(pngData, decodeOutput)
}

/**
 *
 * @param fileData base64 encoded PNG data
 * @param newData new tEXt chunk data
 * @returns base64 encoded PNG with tEXt chunk replaced
 */
export function replacePngTextChunk(
  fileData: string,
  newData: string,
  encodeInput: boolean = true
) {
  return PngUtils.replacePngChunk(fileData, newData, encodeInput)
}
