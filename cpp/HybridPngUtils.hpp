#pragma once
#include <vector>
#include "HybridPngUtilsSpec.hpp"

namespace margelo::nitro::pngutils
{
    class HybridPngUtils : public HybridPngUtilsSpec
    {
    public:
        HybridPngUtils() : HybridObject(TAG), HybridPngUtilsSpec() {}

        std::string extractPngChunk(const std::string &pngBase64, const bool decodeOutput) override;
        std::string replacePngChunk(const std::string &pngData, const std::string &newData, bool encodeInput) override;
    };
} // namespace margelo::nitro::pngutils
