import { NitroModules } from 'react-native-nitro-modules'
import type { PngUtils as PngUtilsSpec } from './specs/png-utils.nitro'

const PngUtils = NitroModules.createHybridObject<PngUtilsSpec>('PngUtils')
/**
 *
 * @param pngData base64 encoded PNG data
 * @returns contents of tEXt chunk
 */
export function getPngChunkText(pngData: string) {
  return PngUtils.getPngChunk(pngData)
}
