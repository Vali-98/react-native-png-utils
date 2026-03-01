#include "HybridPngUtils.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <limits>
#include <zlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace margelo::nitro::pngutils {

// ============================================================
// Base64 (reuse your existing safe implementations if desired)
// ============================================================

extern "C" {
    size_t tb64xdec(const unsigned char *in, size_t inlen, unsigned char *out);
    size_t tb64xenc(const unsigned char *in, size_t inlen, unsigned char *out);
}

static bool base64DecodeToBytes(std::string_view in, std::vector<uint8_t>& out) {
    out.clear();
    if (in.empty()) return true;

    size_t padded = (in.size() + 3) & ~3;
    size_t maxLen = (padded / 4) * 3 + 1;

    if (maxLen > 100 * 1024 * 1024)
        return false;

    out.resize(maxLen);

    size_t len = tb64xdec(
        reinterpret_cast<const unsigned char*>(in.data()),
        in.size(),
        out.data());

    if (!len || len > maxLen)
        return false;

    out.resize(len);
    return true;
}

static bool base64EncodeFromBytes(const std::vector<uint8_t>& in, std::string& out) {
    if (in.empty()) {
        out.clear();
        return true;
    }

    size_t maxLen = (in.size() * 4 / 3) + 4;
    if (maxLen > 100 * 1024 * 1024)
        return false;

    std::vector<uint8_t> tmp(maxLen);

    size_t len = tb64xenc(
        in.data(),
        in.size(),
        tmp.data());

    if (!len || len > maxLen)
        return false;

    out.assign(reinterpret_cast<char*>(tmp.data()), len);
    return true;
}

// ============================================================
// Image Decode (ANY FORMAT)
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
        4); // force RGBA

    if (!data)
        return false;

    if ((uint64_t)width * (uint64_t)height > 100000000ULL) {
        stbi_image_free(data);
        return false;
    }

    size_t size = (size_t)width * height * 4;
    rgbaOut.assign(data, data + size);
    stbi_image_free(data);
    return true;
}

// ============================================================
// PNG Encoding
// ============================================================

static bool encodePng(
    const std::vector<uint8_t>& rgba,
    int width,
    int height,
    std::vector<uint8_t>& pngOut)
{
    pngOut.clear();

    auto callback = [](void* ctx, void* data, int size) {
        auto* buffer = reinterpret_cast<std::vector<uint8_t>*>(ctx);
        uint8_t* bytes = reinterpret_cast<uint8_t*>(data);
        buffer->insert(buffer->end(), bytes, bytes + size);
    };

    int stride = width * 4;

    return stbi_write_png_to_func(
        callback,
        &pngOut,
        width,
        height,
        4,
        rgba.data(),
        stride);
}

// ============================================================
// PNG tEXt Injection
// ============================================================

static void injectTextChunk(
    std::vector<uint8_t>& pngBytes,
    const std::string& text)
{
    static const uint8_t PNG_HEADER[8] =
        {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};

    if (pngBytes.size() < 8 ||
        std::memcmp(pngBytes.data(), PNG_HEADER, 8) != 0)
        throw std::runtime_error("Invalid PNG");

    const uint8_t* begin = pngBytes.data();
    const uint8_t* end   = begin + pngBytes.size();
    const uint8_t* p     = begin + 8;

    // ---- Validate IHDR exists and is first ----
    if (p + 8 > end)
        throw std::runtime_error("Corrupted PNG");

    uint32_t ihdrLen =
        (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
    p += 4;

    if (p + 4 > end || std::strncmp((const char*)p,"IHDR",4)!=0)
        throw std::runtime_error("Invalid PNG: missing IHDR");

    p += 4;

    if (p + ihdrLen + 4 > end)
        throw std::runtime_error("Corrupted IHDR");

    // Move pointer past IHDR data + CRC
    p += ihdrLen + 4;

    size_t insertPos = p - begin;

    // ---- Remove existing tEXt chunks ----
    std::vector<uint8_t> cleaned;
    cleaned.insert(cleaned.end(), begin, begin + 8);

    const uint8_t* scan = begin + 8;

    while (scan + 8 <= end) {
        uint32_t len =
            (scan[0]<<24)|(scan[1]<<16)|
            (scan[2]<<8)|scan[3];

        const uint8_t* type = scan + 4;

        if (scan + 8 + len + 4 > end)
            throw std::runtime_error("Corrupted PNG");

        bool isText =
            (std::strncmp((const char*)type,"tEXt",4)==0);

        if (!isText)
            cleaned.insert(cleaned.end(),
                           scan,
                           scan + 8 + len + 4);

        scan += 8 + len + 4;
    }

    pngBytes.swap(cleaned);

    // ---- Build new tEXt chunk ----
    const std::string keyword = "Comment";
    std::string chunkData = keyword + '\0' + text;

    uint32_t len = (uint32_t)chunkData.size();

    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)"tEXt", 4);
    crc = crc32(crc,
                (const Bytef*)chunkData.data(),
                chunkData.size());

    std::vector<uint8_t> chunk;

    chunk.push_back((len >> 24) & 0xFF);
    chunk.push_back((len >> 16) & 0xFF);
    chunk.push_back((len >> 8) & 0xFF);
    chunk.push_back((len) & 0xFF);

    chunk.insert(chunk.end(), {'t','E','X','t'});

    chunk.insert(chunk.end(),
                 chunkData.begin(),
                 chunkData.end());

    chunk.push_back((crc >> 24) & 0xFF);
    chunk.push_back((crc >> 16) & 0xFF);
    chunk.push_back((crc >> 8) & 0xFF);
    chunk.push_back((crc) & 0xFF);

    // ---- Insert after IHDR ----
    pngBytes.insert(pngBytes.begin() + insertPos,
                    chunk.begin(),
                    chunk.end());
}

// ============================================================
// PNG tEXt Extraction
// ============================================================

static std::string extractTextChunk(
    const std::vector<uint8_t>& pngBytes)
{
    static const uint8_t PNG_HEADER[8] =
        {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};

    if (pngBytes.size() < 8 ||
        std::memcmp(pngBytes.data(), PNG_HEADER, 8) != 0)
        return "";

    const uint8_t* p = pngBytes.data() + 8;
    const uint8_t* end = pngBytes.data() + pngBytes.size();

    while (p + 12 <= end) {
        uint32_t len =
            (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
        p += 4;

        const char* type = (const char*)p;
        p += 4;

        if (p + len + 4 > end)
            return "";

        if (std::strncmp(type, "tEXt", 4) == 0) {
            std::string_view data(
                (const char*)p, len);

            size_t nullPos = data.find('\0');
            if (nullPos != std::string::npos)
                return std::string(
                    data.substr(nullPos + 1));
        }

        p += len + 4;
    }

    return "";
}

// ============================================================
// Public API — NITRO BOUNDARY
// ============================================================

std::string HybridPngUtils::extractPngChunk(
    const std::string& base64Input,
    const bool decodeOutput)
{
    std::vector<uint8_t> bytes;

    if (!base64DecodeToBytes(base64Input, bytes))
        throw std::runtime_error("Invalid image");

    std::string textB64 = extractTextChunk(bytes);

    if (!decodeOutput)
        return textB64;

    // ---- Decode Base64 payload to UTF-8 ----
    std::vector<uint8_t> decoded;

    if (!base64DecodeToBytes(textB64, decoded))
        throw std::runtime_error("Corrupted text payload");

    if (decoded.empty())
        return "";

    return std::string(
        reinterpret_cast<const char*>(decoded.data()),
        decoded.size());
}

std::string HybridPngUtils::replacePngChunk(
    const std::string& base64Input,
    const std::string& newData,
    bool encodeInput)
{
    // ---- Decode image from base64 ----
    std::vector<uint8_t> bytes;

    if (!base64DecodeToBytes(base64Input, bytes))
        throw std::runtime_error("Invalid image");

    // ---- Decode any format to RGBA ----
    int width, height;
    std::vector<uint8_t> rgba;

    if (!decodeAnyImage(bytes, width, height, rgba))
        throw std::runtime_error("Unsupported image");

    // ---- Always re-encode as PNG ----
    std::vector<uint8_t> pngBytes;

    if (!encodePng(rgba, width, height, pngBytes))
        throw std::runtime_error("PNG encode failed");

    // ---- Prepare payload ----
    std::string payloadB64;

    if (encodeInput) {
        // UTF-8 → Base64
        std::vector<uint8_t> inputBytes(
            newData.begin(),
            newData.end());

        if (!base64EncodeFromBytes(inputBytes, payloadB64))
            throw std::runtime_error("Base64 encode failed");
    } else {
        // Already base64
        payloadB64 = newData;
    }

    // ---- Inject chunk ----
    injectTextChunk(pngBytes, payloadB64);

    // ---- Re-encode PNG as base64 ----
    std::string outB64;

    if (!base64EncodeFromBytes(pngBytes, outB64))
        throw std::runtime_error("Base64 encode failed");

    return outB64;
}

} // namespace