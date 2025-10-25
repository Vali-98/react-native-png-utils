#pragma once
#include <vector>
#include "HybridPngUtilsSpec.hpp"

namespace margelo::nitro::pngutils {
class HybridPngUtils : public HybridPngUtilsSpec {
    public:
        HybridPngUtils() : HybridObject(TAG), HybridPngUtilsSpec() {}
       
        std::string getPngChunk(const std::string&  pngBase64) override;
    };
} // namespace margelo::nitro::pngutils
