#pragma once
#include <cstdint>
#include <string>
namespace margelo::nitro::pngutils::audio {
struct Conversion {
    uint32_t sampleRate;
    uint16_t channels;
    uint64_t frames;
    uint64_t bytes;
};
// Implemented separately by Android's NDK media APIs and Apple's AudioToolbox.
Conversion decodeToWav(const std::string& input, const std::string& output);
}
