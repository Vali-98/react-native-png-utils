#pragma once
#include <vector>
#include "HybridPngUtilsSpec.hpp"

namespace margelo::nitro::pngutils {
class HybridPngUtils : public HybridPngUtilsSpec {
    public:
        HybridPngUtils() : HybridObject(TAG), HybridPngUtilsSpec() {}
       
        double sum(double a, double b) override;
    };
} // namespace margelo::nitro::pngutils
