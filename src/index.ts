import { NitroModules } from 'react-native-nitro-modules'
import type {
  AudioUtils as AudioUtilsSpec,
  WavConversionResult as NativeWavConversionResult,
} from './specs/audio-utils.nitro'
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

export type WavConversionResult = NativeWavConversionResult & { uri: string }

function localAudioPath(value: string): string {
  let path = value
  if (!path.startsWith('file:///') && !path.startsWith('file://localhost/'))
    throw new Error('Audio input and output must be local file:// URIs')
  if (path.includes('?') || path.includes('#'))
    throw new Error('File URI path characters must be percent-encoded')
  if (path.startsWith('file:///')) path = decodeURIComponent(path.slice(7))
  else if (path.startsWith('file://localhost/'))
    path = decodeURIComponent(path.slice('file://localhost'.length))
  if (!path.startsWith('/') || path.includes('\0'))
    throw new Error(
      'Audio input and output must be local file:// URIs; copy content:// documents into app-local storage first'
    )
  return path
}

let audioUtils: AudioUtilsSpec | undefined
/** Converts device-supported local audio to a new 16-bit PCM WAV. Requires caller-provided local file:// URIs.
 * Output must not exist; failed conversions remove their incomplete output.
 */
export async function convertToWav(
  inputUri: string,
  outputUri: string
): Promise<WavConversionResult> {
  const input = localAudioPath(inputUri)
  const output = localAudioPath(outputUri)
  audioUtils ??= NitroModules.createHybridObject<AudioUtilsSpec>('AudioUtils')
  const result = await audioUtils.convertToWav(input, output)
  return {
    ...result,
    uri: outputUri,
  }
}
