#include "HybridPngUtils.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <limits>
#include <zlib.h>
#include <optional>
#include <unordered_set>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace margelo::nitro::pngutils {

// ============================================================
// Base64
// ============================================================

extern "C" {
    size_t tb64xdec(const unsigned char *in, size_t inlen, unsigned char *out);
    size_t tb64xenc(const unsigned char *in, size_t inlen, unsigned char *out);
}

static const uint8_t PNG_SIG[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};

// ============================================================
// Chunk model
// ============================================================

struct PngChunk {
    std::string type;
    std::vector<uint8_t> data;
};

// ============================================================
// Base64 helpers
// ============================================================

static bool base64DecodeToBytes(std::string_view in, std::vector<uint8_t>& out) {
    out.clear();
    if (in.empty()) return true;

    size_t padded = (in.size() + 3) & ~3;
    size_t maxLen = (padded / 4) * 3 + 1;

    out.resize(maxLen);

    size_t len = tb64xdec(
        reinterpret_cast<const unsigned char*>(in.data()),
        in.size(),
        out.data());

    if (!len || len > maxLen) return false;

    out.resize(len);
    return true;
}

static bool base64EncodeFromBytes(const std::vector<uint8_t>& in, std::string& out) {
    if (in.empty()) {
        out.clear();
        return true;
    }

    size_t maxLen = (in.size() * 4 / 3) + 4;
    std::vector<uint8_t> tmp(maxLen);

    size_t len = tb64xenc(in.data(), in.size(), tmp.data());
    if (!len || len > maxLen) return false;

    out.assign(reinterpret_cast<char*>(tmp.data()), len);
    return true;
}

// ============================================================
// PNG utils
// ============================================================

static inline uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) |
           (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  |
           (uint32_t(p[3]));
}

static inline void writeBE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

static bool isPNG(const std::vector<uint8_t>& b) {
    return b.size() >= 8 && std::memcmp(b.data(), PNG_SIG, 8) == 0;
}

// ============================================================
// Image decode fallback
// ============================================================

static bool decodeAnyImage(
    const std::vector<uint8_t>& input,
    int& width,
    int& height,
    std::vector<uint8_t>& rgbaOut)
{
    int channels;
    unsigned char* data = stbi_load_from_memory(
        input.data(),
        input.size(),
        &width,
        &height,
        &channels,
        4);

    if (!data) return false;

    if ((uint64_t)width * (uint64_t)height > 100000000ULL) {
        stbi_image_free(data);
        return false;
    }

    size_t size = (size_t)width * height * 4;
    rgbaOut.assign(data, data + size);
    stbi_image_free(data);
    return true;
}

static bool encodePng(
    const std::vector<uint8_t>& rgba,
    int width,
    int height,
    std::vector<uint8_t>& pngOut)
{
    pngOut.clear();

    auto cb = [](void* ctx, void* data, int size) {
        auto* out = reinterpret_cast<std::vector<uint8_t>*>(ctx);
        auto* bytes = reinterpret_cast<uint8_t*>(data);
        out->insert(out->end(), bytes, bytes + size);
    };

    return stbi_write_png_to_func(cb, &pngOut, width, height, 4, rgba.data(), width * 4);
}

// ============================================================
// PNG parsing (NO CRC enforcement here)
// ============================================================

static std::vector<PngChunk> parseChunks(const std::vector<uint8_t>& png) {
    if (!isPNG(png)) throw std::runtime_error("Invalid PNG");

    std::vector<PngChunk> chunks;

    const uint8_t* p = png.data() + 8;
    const uint8_t* end = png.data() + png.size();

    while (p + 8 <= end) {
        uint32_t len = readBE32(p);
        p += 4;

        if (p + 4 > end) break;

        std::string type(reinterpret_cast<const char*>(p), 4);
        p += 4;

        if (p + len + 4 > end) throw std::runtime_error("Corrupt PNG");

        const uint8_t* data = p;
        p += len;

        p += 4; // skip CRC

        chunks.push_back({
            type,
            std::vector<uint8_t>(data, data + len)
        });

        if (type == "IEND") break;
    }

    return chunks;
}

// ============================================================
// PNG build (ONLY CRC point)
// ============================================================

static std::vector<uint8_t> buildPNG(const std::vector<PngChunk>& chunks) {
    std::vector<uint8_t> out;

    out.insert(out.end(), PNG_SIG, PNG_SIG + 8);

    for (const auto& c : chunks) {
        writeBE32(out, (uint32_t)c.data.size());

        out.insert(out.end(), c.type.begin(), c.type.end());
        out.insert(out.end(), c.data.begin(), c.data.end());

        uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, (const Bytef*)c.type.data(), 4);
        crc = crc32(crc, c.data.data(), c.data.size());

        writeBE32(out, crc);
    }

    return out;
}

// ============================================================
// TEXT parsing
// ============================================================

static bool parseText(const PngChunk& c, std::string& k, std::string& v) {
    if (c.type != "tEXt") return false;

    auto it = std::find(c.data.begin(), c.data.end(), 0);
    if (it == c.data.end()) return false;

    k.assign(c.data.begin(), it);
    v.assign(it + 1, c.data.end());
    return true;
}

// ============================================================
// EXTRACT
// ============================================================

std::vector<TextChunkResult>
HybridPngUtils::extractPngChunks(
    const std::string& base64Input,
    const std::optional<ExtractPngChunksOptions>& options)
{
    std::vector<uint8_t> bytes;
    if (!base64DecodeToBytes(base64Input, bytes))
        throw std::runtime_error("Invalid base64 image");

    auto chunks = parseChunks(bytes);

    const auto& opts = options.value_or(ExtractPngChunksOptions{});
    const auto& filter = opts.keywords;
    const bool decode = opts.decodeBase64.value_or(true);

    std::vector<TextChunkResult> result;

    for (const auto& c : chunks) {
        if (c.type != "tEXt") continue;

        std::string k, v;
        if (!parseText(c, k, v)) continue;

        if (filter.has_value()) {
            bool ok = false;
            for (const auto& f : *filter)
                if (f == k) ok = true;
            if (!ok) continue;
        }

        std::string finalValue = v;

        if (decode) {
            std::vector<uint8_t> decoded;
            if (base64DecodeToBytes(v, decoded)) {
                finalValue.assign((char*)decoded.data(), decoded.size());
            }
        }

        result.push_back(TextChunkResult{k, finalValue});
    }

    return result;
}

// ============================================================
// REPLACE
// ============================================================

std::string
HybridPngUtils::replacePngChunks(
    const std::string& base64Input,
    const std::vector<TextChunk>& chunks,
    const std::optional<ReplacePngChunksOptions>& options)
{
    std::vector<uint8_t> bytes;

    if (!base64DecodeToBytes(base64Input, bytes))
        throw std::runtime_error("Invalid base64 image");

    bool wasPNG = isPNG(bytes);

    if (!wasPNG) {
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;

        if (!decodeAnyImage(bytes, w, h, rgba))
            throw std::runtime_error("Decode failed");

        if (!encodePng(rgba, w, h, bytes))
            throw std::runtime_error("PNG encode failed");
    }

    auto parsed = parseChunks(bytes);

    std::unordered_set<std::string> removeSet;

    if (options && options->removeKeywords.has_value()) {
        for (const auto& k : *options->removeKeywords)
            removeSet.insert(k);
    }

    for (const auto& c : chunks)
        removeSet.insert(c.keyword);

    std::vector<PngChunk> beforeIDAT;
    std::vector<PngChunk> afterIDAT;

    bool reachedIDAT = false;

    for (const auto& c : parsed) {

        if (c.type == "IEND") {
            afterIDAT.push_back(c);
            continue;
        }

        if (c.type == "IDAT")
            reachedIDAT = true;

        if (c.type == "tEXt") {
            std::string k, v;
            if (parseText(c, k, v)) {
                if (removeSet.count(k)) continue;
            }
        }

        if (!reachedIDAT)
            beforeIDAT.push_back(c);
        else
            afterIDAT.push_back(c);
    }

    for (const auto& nc : chunks) {
        std::vector<uint8_t> payload;

        payload.insert(payload.end(), nc.keyword.begin(), nc.keyword.end());
        payload.push_back(0);

        std::string value = nc.data;

        if (nc.b64encode) {
            std::vector<uint8_t> encoded;

            size_t maxLen = (value.size() * 4 / 3) + 4;
            encoded.resize(maxLen);

            size_t len = tb64xenc(
                reinterpret_cast<const unsigned char*>(value.data()),
                value.size(),
                encoded.data()
            );

            if (!len) {
                throw std::runtime_error("Base64 encode failed for tEXt payload");
            }

            payload.insert(payload.end(),
                        encoded.begin(),
                        encoded.begin() + len);
        } else {
            payload.insert(payload.end(),
                        value.begin(),
                        value.end());
        }


        PngChunk t;
        t.type = "tEXt";
        t.data = std::move(payload);

        beforeIDAT.push_back(std::move(t));
    }

    std::vector<PngChunk> out;
    out.reserve(beforeIDAT.size() + afterIDAT.size());

    out.insert(out.end(), beforeIDAT.begin(), beforeIDAT.end());
    out.insert(out.end(), afterIDAT.begin(), afterIDAT.end());

    auto png = buildPNG(out);

    std::string outB64;
    if (!base64EncodeFromBytes(png, outB64))
        throw std::runtime_error("Base64 encode failed");

    return outB64;
}

} // namespace