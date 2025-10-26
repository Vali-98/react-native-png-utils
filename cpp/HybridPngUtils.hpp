#pragma once
#include <vector>
#include "HybridPngUtilsSpec.hpp"

namespace margelo::nitro::pngutils
{
    class HybridPngUtils : public HybridPngUtilsSpec
    {
    public:
        HybridPngUtils() : HybridObject(TAG), HybridPngUtilsSpec() {}

        std::string extractPngChunk(const std::string &pngBase64) override;
        std::string replacePngChunk(const std::string &pngData, const std::string &newData) override;
    };
} // namespace margelo::nitro::pngutils
