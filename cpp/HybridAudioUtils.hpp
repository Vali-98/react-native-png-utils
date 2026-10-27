#pragma once
#include "HybridAudioUtilsSpec.hpp"
namespace margelo::nitro::pngutils {
class HybridAudioUtils final : public HybridAudioUtilsSpec {
public:
    HybridAudioUtils() : HybridObject(TAG), HybridAudioUtilsSpec() {}
    std::shared_ptr<Promise<WavConversionResult>> convertToWav(
        const std::string& inputPath, const std::string& outputPath) override;
};
}
