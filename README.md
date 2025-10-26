# @vali98/react-native-png-utils

react-native-png-utils provides functions for extracting and replacing CharacterCardV2 data from PNG tEXt chunks

This library is mainly intended for use in [ChatterUI](https://github.com/Vali-98/ChatterUI), but could prove useful in other apps utlizing CharacterCardV2 data.

## Requirements

- React Native v0.76.0 or higher
- Node 18.0.0 or higher

## Installation

```bash
npm install @vali98/react-native-png-utils react-native-nitro-modules
```

## Usage

```ts
import {
  extractPngTextChunk,
  replacePngTextChunk,
} from '@vali98/react-native-png-utils'

const textChunk = extractPngTextChunk(base64EncodedPNGData)

const newPNGData = replacePngTextChunk(base64EncodedPNGData, newUTF8tEXtData)
```

## Benchmarks

Here are some comparisons for the performance of this library vs a naive JS implementation used in ChatterUI:

#### Extracting tEXT Chunk Performance

| Implementation | Time Taken (5k tokens) |
| :------------- | :--------------------- |
| **JS**         | $\sim 10\text{ms}$     |
| **png-utils**  | $\sim 0.1\text{ms}$    |

#### Replacing tEXT Chunk Performance

| Implementation | Time Taken (5k tokens) |
| :------------- | :--------------------- |
| **JS**         | $\sim 30\text{ms}$     |
| **png-utils**  | $\sim 1\text{ms}$      |

## Credits

- Bootstrapped with [create-nitro-module](https://github.com/patrickkabwe/create-nitro-module).
- Utilizes [Turbo-Base64](https://github.com/powturbo/Turbo-Base64) for encoding/decoding with SIMD support.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
