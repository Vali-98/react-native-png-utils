#pragma once

// Minimal binding substitute for exercising the production implementation on
// the host without a React Native runtime. Android builds use the real spec.
#include <optional>
#include <string>
#include <vector>

namespace margelo::nitro::pngutils {
struct TextChunk {
    std::string keyword;
    std::string data;
    std::optional<bool> b64encode;
};
struct TextChunkResult { std::string keyword; std::string data; };
struct ExtractPngChunksOptions {
    std::optional<std::vector<std::string>> keywords;
    std::optional<bool> decodeBase64;
};
struct ReplacePngChunksOptions {
    std::optional<std::vector<std::string>> removeKeywords;
};
struct HybridObject { explicit HybridObject(const char*) {} };
struct HybridPngUtilsSpec : virtual HybridObject {
    static constexpr auto TAG = "PngUtils";
    HybridPngUtilsSpec() : HybridObject(TAG) {}
    virtual ~HybridPngUtilsSpec() = default;
    virtual std::string replacePngChunks(const std::string&, const std::vector<TextChunk>&,
                                        const std::optional<ReplacePngChunksOptions>&) = 0;
    virtual std::vector<TextChunkResult> extractPngChunks(const std::string&,
                                        const std::optional<ExtractPngChunksOptions>&) = 0;
};
}
