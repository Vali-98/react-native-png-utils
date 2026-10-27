#include "HybridAudioUtils.hpp"
#include "audio/AudioDecoder.hpp"
#include <stdexcept>
namespace margelo::nitro::pngutils {
std::shared_ptr<Promise<WavConversionResult>> HybridAudioUtils::convertToWav(
    const std::string& inputPath, const std::string& outputPath) {
    // Copy arguments into the worker; never capture this or JS references.
    return Promise<WavConversionResult>::async([inputPath, outputPath]() {
        if (inputPath.empty() || inputPath[0] != '/' || inputPath.find('\0') != std::string::npos)
            throw std::runtime_error("Input must be an absolute local filesystem path; copy content/file URIs first");
        const auto result = audio::decodeToWav(inputPath, outputPath);
        WavConversionResult value;
        value.path = outputPath;
        value.sampleRate = result.sampleRate;
        value.channels = result.channels;
        value.frames = static_cast<double>(result.frames);
        value.durationSeconds = static_cast<double>(result.frames) / result.sampleRate;
        value.bytes = static_cast<double>(result.bytes);
        return value;
    });
}
}
