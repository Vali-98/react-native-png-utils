import { NitroModules } from 'react-native-nitro-modules'
import type { PngUtils as PngUtilsSpec } from './specs/png-utils.nitro'

export const PngUtils =
  NitroModules.createHybridObject<PngUtilsSpec>('PngUtils')