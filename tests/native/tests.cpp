#include "fixtures.hpp"
#include <iostream>
#include <functional>
using namespace margelo::nitro::pngutils;
using namespace fixture;

void rejects(const std::function<void()>& action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Expected rejection");
}

int main() {
    HybridPngUtils utils;
    const auto input = encode(png());
    require(utils.extractPngChunks(input, {}).empty(), "Empty metadata");
    require(utils.replacePngChunks(input, {}, {}) == input, "No-op changed PNG");
    for (size_t n = 0; n <= 1024; ++n) {
        std::string value(n, '\0');
        for (size_t i = 0; i < n; ++i) value[i] = char((i * 73 + n) & 255);
        const auto output = utils.replacePngChunks(input, {{"chara", value, true}}, {});
        validCRCs(decode(output));
        const auto found = utils.extractPngChunks(output, {});
        require(found.size() == 1 && found[0].data == value, "Base64 boundary round trip");
        ExtractPngChunksOptions raw;
        raw.decodeBase64 = false;
        require(utils.extractPngChunks(output, raw)[0].data == encode(Bytes(value.begin(), value.end())),
                "SIMD encoder differs from reference");
        // Explicit false must not encode the value.
        const auto plain = utils.replacePngChunks(input, {{"chara", value, false}}, {});
        require(utils.extractPngChunks(plain, raw)[0].data == value, "False b64encode flag ignored");
    }
    for (const auto& bad : {"A", "AA", "AAA", "A===", "====", "AA=A", "=AAA", "A=AA"})
        rejects([&] { utils.extractPngChunks(bad, {}); });
    // Check every position across SIMD and scalar unrolled blocks, including
    // errors the bundled decoder's old partial validation missed.
    for (size_t i = 0; i < 1024; ++i) {
        std::string invalid(1024, 'A');
        invalid[i] = '!';
        const auto found = utils.extractPngChunks(encode(withText("raw", invalid)), {});
        require(found[0].data == invalid, "Invalid text base64 was decoded");
    }
    auto image = png();
    for (size_t n = 0; n < image.size(); ++n) {
        const auto truncated = encode(Bytes(image.begin(), image.begin() + n));
        rejects([&] { utils.extractPngChunks(truncated, {}); });
        rejects([&] { utils.replacePngChunks(truncated, {{"x", "y", false}}, {}); });
    }
    auto huge = image;
    huge[33] = 0x7f; huge[34] = huge[35] = huge[36] = 0xff;
    rejects([&] { utils.extractPngChunks(encode(huge), {}); });
    auto metadata = withText("old", "before");
    metadata.resize(metadata.size() - 12);
    text(metadata, "keep", "after");
    text(metadata, "old", "duplicate");
    chunk(metadata, "IEND", {});
    ReplacePngChunksOptions removal;
    removal.removeKeywords = std::vector<std::string>{"old"};
    const auto output = utils.replacePngChunks(encode(metadata), {{"new", "value", {}}}, removal);
    validCRCs(decode(output));
    ExtractPngChunksOptions raw;
    raw.decodeBase64 = false;
    const auto found = utils.extractPngChunks(output, raw);
    require(found.size() == 2 && found[0].keyword == "new" && found[1].keyword == "keep", "Replacement order/removal");
    raw.keywords = std::vector<std::string>{"keep"};
    require(utils.extractPngChunks(output, raw).size() == 1, "Keyword filter");
    raw.keywords = std::vector<std::string>{};
    require(utils.extractPngChunks(output, raw).empty(), "Empty keyword filter");
    rejects([&] { utils.replacePngChunks(input, {{"", "value", {}}}, {}); });
    rejects([&] { utils.replacePngChunks(input, {{std::string(80, 'x'), "value", {}}}, {}); });
    // Retain existing chunks byte-for-byte, including their CRC fields.
    auto preserved = png();
    preserved[29] ^= 1;
    const auto edited = decode(utils.replacePngChunks(encode(preserved), {{"x", "y", false}}, {}));
    require(std::equal(preserved.begin() + 8, preserved.begin() + 33, edited.begin() + 8), "IHDR changed");
    // Non-PNG conversion: a one-pixel binary PPM, then verify pixels using STB
    // in a separate translation unit to avoid relying only on metadata parsing.
    std::string ppm = "P6\n1 1\n255\n";
    ppm.append("\x11\x22\x33", 3);
    const auto converted = utils.replacePngChunks(encode(Bytes(ppm.begin(), ppm.end())), {{"x", "y", false}}, {});
    validCRCs(decode(converted));
    extern void checkPixel(const Bytes&);
    checkPixel(decode(converted));
    std::string oversized = "P6\n100001 1000\n255\n";
    rejects([&] { utils.replacePngChunks(encode(Bytes(oversized.begin(), oversized.end())), {}, {}); });
    std::cout << "Native tests passed (base64 boundaries, malformed input, metadata edits, CRCs, conversion)\n";
}
