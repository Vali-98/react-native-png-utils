#pragma once
#include "AudioDecoder.hpp"
#include <string>
#include <cstdio>
namespace margelo::nitro::pngutils::audio {
// Streams little-endian signed 16-bit PCM directly to an app-local file.
// Exclusive creation prevents overwriting; failed conversions remove the output.
class WavWriter final {
public:
    WavWriter(const std::string& output, uint32_t rate, uint16_t channels);
    ~WavWriter();
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    void write(const void* pcm, size_t bytes);
    Conversion finish();
private:
    std::string output_, pending_;
    FILE* file_ = nullptr;
    uint32_t rate_;
    uint16_t channels_;
    uint64_t bytes_ = 0;
};
}
