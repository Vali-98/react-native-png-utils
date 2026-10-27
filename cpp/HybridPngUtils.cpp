#include "HybridPngUtils.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <zlib.h>
#include "turbob64/turbob64.h"

// zlib is already linked for CRCs. Reuse its compressor instead of embedding
// STB's separate deflate implementation.
static unsigned char* compressPNG(unsigned char* data, int size, int* outputSize, int quality) {
    uLongf length = compressBound(static_cast<uLong>(size));
    if (length > static_cast<uLongf>(std::numeric_limits<int>::max())) return nullptr;
    auto* out = static_cast<unsigned char*>(std::malloc(length));
    if (!out) return nullptr;
    if (compress2(out, &length, data, static_cast<uLong>(size), std::clamp(quality, 0, 9)) != Z_OK) {
        std::free(out);
        return nullptr;
    }
    *outputSize = static_cast<int>(length);
    return out;
}

// Keep only the STB entry points used here; avoid exporting unused image writers
// and filesystem APIs from the native library.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#define STBIW_ZLIB_COMPRESS compressPNG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace margelo::nitro::pngutils {
namespace {
constexpr uint8_t PNG_SIG[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
constexpr size_t MAX_CHUNK_SIZE = 0x7fffffff;

size_t checkedAdd(size_t a, size_t b) {
    if (b > std::numeric_limits<size_t>::max() - a)
        throw std::runtime_error("Image too large");
    return a + b;
}

size_t base64Size(size_t size) {
    const size_t groups = checkedAdd(size, 2) / 3;
    if (groups > std::numeric_limits<size_t>::max() / 4)
        throw std::runtime_error("Image too large");
    return groups * 4;
}

size_t encodeBase64(const uint8_t* in, size_t size, uint8_t* out) {
    if (size == 0) return 0;
#if defined(PNGUTIL_ENABLE_NEON) && defined(__aarch64__)
    // The bundled SIMD encoder rounds its loop by OUTPUT length. Feed it only
    // complete 48-byte blocks so a padded last block cannot overread the input.
    const size_t prefix = size / 48 * 48;
    const size_t written = prefix ? tb64v128enc(in, prefix, out) : 0;
    return written + (prefix < size ? tb64xenc(in + prefix, size - prefix, out + written) : 0);
#else
    return tb64xenc(in, size, out);
#endif
}

template <typename Buffer>
bool decodeBase64(std::string_view in, Buffer& out) {
    out.clear();
    if (in.empty()) return true;
    // Turbo-Base64 expects complete quartets, even for its smallest inputs.
    if ((in.size() & 3) != 0) return false;
    const size_t n = in.size();
    if (in[n - 4] == '=' || in[n - 3] == '=' ||
        (in[n - 2] == '=' && in[n - 1] != '=')) return false;

    // Scalar decoding may store one extra byte while unpacking a quartet.
    out.resize(checkedAdd(n / 4 * 3, 1));
    auto* dst = reinterpret_cast<uint8_t*>(out.data());
    const auto* src = reinterpret_cast<const uint8_t*>(in.data());
    size_t prefix = 0, written = 0;
#if defined(PNGUTIL_ENABLE_NEON) && defined(__aarch64__)
    // Keep the final quartet in the scalar tail: SIMD does not handle padding.
    prefix = (n - 4) / 64 * 64;
    if (prefix) {
        written = tb64v128dec(src, prefix, dst);
        if (written != prefix / 4 * 3) { out.clear(); return false; }
    }
#endif
    const size_t tail = tb64xdec(src + prefix, n - prefix, dst + written);
    if (!tail) { out.clear(); return false; }
    out.resize(written + tail);
    return true;
}

std::string encodeBase64(const std::vector<uint8_t>& bytes) {
    std::string out(base64Size(bytes.size()), '\0');
    if (encodeBase64(bytes.data(), bytes.size(), reinterpret_cast<uint8_t*>(out.data())) != out.size())
        throw std::runtime_error("Base64 encode failed");
    return out;
}

uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

void writeBE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((v >> 24) & 0xff);
    out.push_back((v >> 16) & 0xff);
    out.push_back((v >> 8) & 0xff);
    out.push_back(v & 0xff);
}

bool isPNG(const std::vector<uint8_t>& bytes) {
    return bytes.size() >= sizeof(PNG_SIG) &&
           std::memcmp(bytes.data(), PNG_SIG, sizeof(PNG_SIG)) == 0;
}

struct ChunkView {
    const uint8_t* raw;
    size_t size;
    bool is(const char* type) const { return std::memcmp(raw + 4, type, 4) == 0; }
    std::string_view text() const {
        return {reinterpret_cast<const char*>(raw + 8), size - 12};
    }
};

// Validate framing without copying payloads or allocating a chunk collection.
// Existing CRCs are preserved, not validated or silently repaired.
template <typename Visitor>
void visitChunks(const std::vector<uint8_t>& png, Visitor&& visit) {
    if (!isPNG(png)) throw std::runtime_error("Invalid PNG");
    size_t offset = sizeof(PNG_SIG);
    bool header = false, idat = false;
    while (png.size() - offset >= 12) {
        const auto* raw = png.data() + offset;
        const size_t length = readBE32(raw);
        if (length > MAX_CHUNK_SIZE || length > png.size() - offset - 12)
            throw std::runtime_error("Corrupt PNG chunk length");
        const ChunkView chunk{raw, length + 12};
        if (!header) {
            if (!chunk.is("IHDR") || length != 13)
                throw std::runtime_error("Missing PNG header");
            header = true;
        } else if (chunk.is("IHDR")) {
            throw std::runtime_error("Duplicate PNG header");
        }
        if (chunk.is("IDAT")) idat = true;
        if (chunk.is("IEND")) {
            if (length != 0 || !idat) throw std::runtime_error("Corrupt PNG end");
            visit(chunk);
            return;
        }
        visit(chunk);
        offset += chunk.size;
    }
    throw std::runtime_error("Missing PNG end");
}

void convertToPNG(std::vector<uint8_t>& bytes) {
    if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Image too large to decode");
    const int size = static_cast<int>(bytes.size());
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(bytes.data(), size, &width, &height, &channels) ||
        width <= 0 || height <= 0 || uint64_t(width) * uint64_t(height) > 100000000ULL)
        throw std::runtime_error("Invalid image dimensions");
    using Pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    Pixels pixels(stbi_load_from_memory(bytes.data(), size, &width, &height, &channels, 4), stbi_image_free);
    if (!pixels) throw std::runtime_error("Decode failed");

    // Drop the compressed source before allocating PNG encoder workspaces.
    std::vector<uint8_t>().swap(bytes);
    auto callback = [](void* ctx, void* data, int length) {
        auto& out = *static_cast<std::vector<uint8_t>*>(ctx);
        const auto* begin = static_cast<const uint8_t*>(data);
        out.insert(out.end(), begin, begin + length);
    };
    if (!stbi_write_png_to_func(callback, &bytes, width, height, 4, pixels.get(), width * 4))
        throw std::runtime_error("PNG encode failed");
}

size_t payloadSize(const TextChunk& chunk) {
    if (chunk.keyword.empty() || chunk.keyword.size() > 79 || chunk.keyword.find('\0') != std::string::npos)
        throw std::runtime_error("PNG keyword must contain 1 to 79 bytes without NUL");
    const size_t valueSize = chunk.b64encode.value_or(false) ? base64Size(chunk.data.size()) : chunk.data.size();
    const size_t size = checkedAdd(chunk.keyword.size() + 1, valueSize);
    if (size > MAX_CHUNK_SIZE) throw std::runtime_error("PNG text chunk too large");
    return size;
}

void appendText(std::vector<uint8_t>& out, const TextChunk& chunk) {
    const size_t size = payloadSize(chunk);
    writeBE32(out, static_cast<uint32_t>(size));
    const size_t typeOffset = out.size();
    out.insert(out.end(), {'t', 'E', 'X', 't'});
    out.insert(out.end(), chunk.keyword.begin(), chunk.keyword.end());
    out.push_back(0);
    if (chunk.b64encode.value_or(false)) {
        const size_t offset = out.size();
        out.resize(offset + base64Size(chunk.data.size()));
        encodeBase64(reinterpret_cast<const uint8_t*>(chunk.data.data()), chunk.data.size(), out.data() + offset);
    } else {
        out.insert(out.end(), chunk.data.begin(), chunk.data.end());
    }
    uLong crc = crc32(0L, Z_NULL, 0);
    // Split type and payload so size + 4 cannot overflow zlib's uInt on 32-bit.
    crc = crc32(crc, out.data() + typeOffset, 4);
    crc = crc32(crc, out.data() + typeOffset + 4, static_cast<uInt>(size));
    writeBE32(out, static_cast<uint32_t>(crc));
}
} // namespace

std::vector<TextChunkResult> HybridPngUtils::extractPngChunks(
    const std::string& base64Input,
    const std::optional<ExtractPngChunksOptions>& options) {
    std::vector<uint8_t> bytes;
    if (!decodeBase64(base64Input, bytes)) throw std::runtime_error("Invalid base64 image");
    const bool decode = !options || options->decodeBase64.value_or(true);
    std::vector<TextChunkResult> result;
    visitChunks(bytes, [&](const ChunkView& chunk) {
        if (!chunk.is("tEXt")) return;
        const auto text = chunk.text();
        const size_t separator = text.find('\0');
        if (separator == std::string_view::npos) return;
        const auto key = text.substr(0, separator);
        if (options && options->keywords &&
            std::find(options->keywords->begin(), options->keywords->end(), key) == options->keywords->end()) return;
        const auto value = text.substr(separator + 1);
        result.emplace_back();
        auto& entry = result.back();
        entry.keyword.assign(key.data(), key.size());
        if (!decode || !decodeBase64(value, entry.data)) entry.data.assign(value.data(), value.size());
    });
    return result;
}

std::string HybridPngUtils::replacePngChunks(
    const std::string& base64Input,
    const std::vector<TextChunk>& chunks,
    const std::optional<ReplacePngChunksOptions>& options) {
    std::vector<uint8_t> bytes;
    if (!decodeBase64(base64Input, bytes)) throw std::runtime_error("Invalid base64 image");
    const bool originalPNG = isPNG(bytes);
    if (!originalPNG) convertToPNG(bytes);

    std::unordered_set<std::string_view> remove;
    const size_t removeCount = options && options->removeKeywords ? options->removeKeywords->size() : 0;
    remove.reserve(checkedAdd(removeCount, chunks.size()));
    if (options && options->removeKeywords)
        for (const auto& key : *options->removeKeywords) remove.insert(key);
    size_t outputSize = sizeof(PNG_SIG);
    for (const auto& chunk : chunks) {
        remove.insert(chunk.keyword);
        outputSize = checkedAdd(outputSize, checkedAdd(payloadSize(chunk), 12));
    }
    const auto removed = [&](const ChunkView& chunk) {
        if (!chunk.is("tEXt") || remove.empty()) return false;
        const auto text = chunk.text();
        const size_t separator = text.find('\0');
        return separator != std::string_view::npos &&
               remove.find(text.substr(0, separator)) != remove.end();
    };
    bool changed = !chunks.empty();
    visitChunks(bytes, [&](const ChunkView& chunk) {
        if (removed(chunk)) changed = true;
        else outputSize = checkedAdd(outputSize, chunk.size);
    });
    if (originalPNG && !changed) return base64Input;

    std::vector<uint8_t> out;
    out.reserve(outputSize);
    out.insert(out.end(), std::begin(PNG_SIG), std::end(PNG_SIG));
    bool inserted = false;
    visitChunks(bytes, [&](const ChunkView& chunk) {
        if (!inserted && chunk.is("IDAT")) {
            for (const auto& text : chunks) appendText(out, text);
            inserted = true;
        }
        if (!removed(chunk)) out.insert(out.end(), chunk.raw, chunk.raw + chunk.size);
    });
    // Binary input is no longer needed while the final base64 string is allocated.
    std::vector<uint8_t>().swap(bytes);
    return encodeBase64(out);
}
} // namespace margelo::nitro::pngutils
