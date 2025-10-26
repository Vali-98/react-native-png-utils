#include "HybridPngUtils.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <zlib.h>

namespace margelo::nitro::pngutils
{

    extern "C"
    {
        size_t tb64xdec(const unsigned char *in, size_t inlen, unsigned char *out);
        size_t tb64xenc(const unsigned char *in, size_t inlen, unsigned char *out);
        size_t tb64v128dec(const unsigned char *in, size_t inlen, unsigned char *out);
        size_t tb64v128enc(const unsigned char *in, size_t inlen, unsigned char *out);
    }

    static inline bool base64DecodeToBytes(std::string_view in, std::vector<uint8_t> &out)
    {
        out.clear();
        if (in.empty())
            return true;
        size_t outLen;
        out.resize((in.size() / 4) * 3 + 4);
#if defined(PNGUTIL_ENABLE_NEON)
        outLen = tb64v128dec(
            reinterpret_cast<const unsigned char *>(in.data()),
            in.size(),
            out.data());
#else
        // Fallback scalar path
        outLen = tb64xdec(
            reinterpret_cast<const unsigned char *>(in.data()),
            in.size(),
            out.data());
#endif
        if (!outLen)
            return false;

        out.resize(outLen);
        return true;
    }

    static inline bool base64EncodeFromBytes(const std::vector<uint8_t> &in, std::string &out)
    {
        if (in.empty())
        {
            out.clear();
            return true;
        }

        std::vector<uint8_t> tmp;
        tmp.resize((in.size() * 4 / 3) + 8);
        size_t outLen;

#if defined(PNGUTIL_ENABLE_NEON)
        outLen = tb64v128enc(in.data(), in.size(), tmp.data());
#else
        outLen = tb64xenc(in.data(), in.size(), tmp.data());
#endif
        if (!outLen)
            return false;

        out.assign(reinterpret_cast<const char *>(tmp.data()), outLen);
        return true;
    }

    static inline uint32_t readUInt32BE(const uint8_t *p)
    {
        return (uint32_t(p[0]) << 24) |
               (uint32_t(p[1]) << 16) |
               (uint32_t(p[2]) << 8) |
               uint32_t(p[3]);
    }

    std::string HybridPngUtils::extractPngChunk(const std::string &base64Png)
    {
        static thread_local std::vector<uint8_t> bytes;
        bytes.clear();

        if (!base64DecodeToBytes(base64Png, bytes))
            throw std::runtime_error("Invalid Base64 PNG data");

        // Validate PNG header
        if (bytes.size() < 8)
            throw std::runtime_error("Invalid PNG: too short");

        static const uint8_t PNG_HEADER[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (std::memcmp(bytes.data(), PNG_HEADER, 8) != 0)
            throw std::runtime_error("Invalid PNG header");

        const uint8_t *p = bytes.data() + 8;
        const uint8_t *end = bytes.data() + bytes.size();

        while (p + 12 <= end)
        { // length(4) + type(4) + crc(4)
            uint32_t length = readUInt32BE(p);
            p += 4;
            if (p + 4 > end)
                break;

            std::string_view typeSv(reinterpret_cast<const char *>(p), 4);
            p += 4;

            if (p + length + 4 > end)
                throw std::runtime_error("Corrupted PNG: chunk exceeds file size");

            std::string_view chunkSv(reinterpret_cast<const char *>(p), length);
            p += length + 4; // skip CRC

            if (typeSv == "tEXt")
            {
                size_t nullPos = chunkSv.find('\0');
                if (nullPos == std::string_view::npos)
                    throw std::runtime_error("Malformed tEXt chunk: missing null separator");

                std::string_view valueSv = chunkSv.substr(nullPos + 1);

                std::vector<uint8_t> tmp;
                tmp.reserve((valueSv.size() / 4) * 3 + 4);

                if (!base64DecodeToBytes(valueSv, tmp))
                    throw std::runtime_error("Failed to decode tEXt chunk value");

                std::string result(reinterpret_cast<const char *>(tmp.data()), tmp.size());
                while (!result.empty() && (unsigned char)result.back() <= 0x1F)
                    result.pop_back();
                return result;
            }
        }

        throw std::runtime_error("tEXt chunk not found");
    }

   std::string HybridPngUtils::replacePngChunk(const std::string &base64Png,
                                                    const std::string &newText)
    {
        // ---- Decode PNG ----
        std::vector<uint8_t> bytes;
        if (!base64DecodeToBytes(base64Png, bytes))
            throw std::runtime_error("Invalid Base64 PNG data");
        if (bytes.size() < 8)
            throw std::runtime_error("Invalid PNG: too short");

        static const uint8_t PNG_HEADER[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (std::memcmp(bytes.data(), PNG_HEADER, 8) != 0)
            throw std::runtime_error("Invalid PNG header");

        // ---- Encode new UTF-8 text as Base64 ----
        std::vector<uint8_t> utf8bytes(newText.begin(), newText.end());
        std::string newTextB64;
        if (!base64EncodeFromBytes(utf8bytes, newTextB64))
            throw std::runtime_error("Failed to base64 encode new text");

        // ---- Iterate chunks ----
        const uint8_t *ptr = bytes.data() + 8;
        const uint8_t *end = bytes.data() + bytes.size();
        std::vector<uint8_t> newBytes;
        newBytes.insert(newBytes.end(), bytes.begin(), bytes.begin() + 8); // keep PNG header

        bool replaced = false;
        bool inserted = false;

        while (ptr + 12 <= end)
        {
            uint32_t length = readUInt32BE(ptr);
            const uint8_t *chunkStart = ptr;
            ptr += 4;
            if (ptr + 4 > end)
                throw std::runtime_error("Invalid PNG chunk header");

            const char *type = reinterpret_cast<const char *>(ptr);
            ptr += 4;
            const uint8_t *dataStart = ptr;
            ptr += length;
            if (ptr + 4 > end)
                throw std::runtime_error("Invalid PNG chunk length");
            ptr += 4;

            // ---- Replace existing tEXt ----
            if (!replaced && std::strncmp(type, "tEXt", 4) == 0)
            {
                // Copy up to start of this chunk
                size_t prefixLen = chunkStart - bytes.data();
                newBytes.resize(prefixLen);

                // Read keyword up to null
                size_t nullPos = 0;
                while (nullPos < length && dataStart[nullPos] != 0)
                    ++nullPos;
                if (nullPos >= length)
                    throw std::runtime_error("Malformed tEXt chunk");

                std::string keyword(reinterpret_cast<const char *>(dataStart), nullPos + 1);
                std::string newChunkData = keyword + newTextB64;
                uint32_t newLen = static_cast<uint32_t>(newChunkData.size());

                // Write new chunk
                newBytes.push_back((newLen >> 24) & 0xFF);
                newBytes.push_back((newLen >> 16) & 0xFF);
                newBytes.push_back((newLen >> 8) & 0xFF);
                newBytes.push_back((newLen)&0xFF);
                newBytes.insert(newBytes.end(), {'t', 'E', 'X', 't'});
                newBytes.insert(newBytes.end(), newChunkData.begin(), newChunkData.end());

                uint32_t newCrc = crc32(0L, Z_NULL, 0);
                newCrc = crc32(newCrc, reinterpret_cast<const Bytef *>("tEXt"), 4);
                newCrc = crc32(newCrc, reinterpret_cast<const Bytef *>(newChunkData.data()), newChunkData.size());
                newBytes.push_back((newCrc >> 24) & 0xFF);
                newBytes.push_back((newCrc >> 16) & 0xFF);
                newBytes.push_back((newCrc >> 8) & 0xFF);
                newBytes.push_back((newCrc)&0xFF);

                replaced = true;
                continue; // skip copying old chunk
            }

            // ---- If no tEXt found, insert before IEND ----
            if (!inserted && std::strncmp(type, "IEND", 4) == 0 && !replaced)
            {
                std::string keyword = "Comment";
                std::string newChunkData = keyword + '\0' + newTextB64;
                uint32_t newLen = static_cast<uint32_t>(newChunkData.size());

                newBytes.push_back((newLen >> 24) & 0xFF);
                newBytes.push_back((newLen >> 16) & 0xFF);
                newBytes.push_back((newLen >> 8) & 0xFF);
                newBytes.push_back((newLen)&0xFF);
                newBytes.insert(newBytes.end(), {'t', 'E', 'X', 't'});
                newBytes.insert(newBytes.end(), newChunkData.begin(), newChunkData.end());

                uint32_t newCrc = crc32(0L, Z_NULL, 0);
                newCrc = crc32(newCrc, reinterpret_cast<const Bytef *>("tEXt"), 4);
                newCrc = crc32(newCrc, reinterpret_cast<const Bytef *>(newChunkData.data()), newChunkData.size());
                newBytes.push_back((newCrc >> 24) & 0xFF);
                newBytes.push_back((newCrc >> 16) & 0xFF);
                newBytes.push_back((newCrc >> 8) & 0xFF);
                newBytes.push_back((newCrc)&0xFF);

                inserted = true;
            }

            // ---- Copy original chunk ----
            newBytes.insert(newBytes.end(), chunkStart, ptr);
        }

        if (!replaced && !inserted)
            throw std::runtime_error("No IEND chunk found or failed to insert");

        // ---- Re-encode as Base64 ----
        std::string resultB64;
        if (!base64EncodeFromBytes(newBytes, resultB64))
            throw std::runtime_error("Failed to base64 encode result PNG");

        return resultB64;
    }
} // namespace margelo::nitro::pngutils
