#pragma once
#include <vector>
#include "HybridPngUtilsSpec.hpp"

namespace margelo::nitro::pngutils
{
    class HybridPngUtils : public HybridPngUtilsSpec
    {
    public:
        HybridPngUtils() : HybridObject(TAG), HybridPngUtilsSpec() {}

            virtual std::string replacePngChunks(const std::string& imageBase64, const std::vector<TextChunk>& chunks, const std::optional<ReplacePngChunksOptions>& options) override;
            virtual std::vector<TextChunkResult> extractPngChunks(const std::string& imageBase64, const std::optional<ExtractPngChunksOptions>& options) override;
    };
} // namespace margelo::nitro::pngutils
