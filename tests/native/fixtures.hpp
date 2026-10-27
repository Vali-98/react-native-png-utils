#pragma once
#include "HybridPngUtils.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

namespace fixture {
using Bytes = std::vector<uint8_t>;
inline void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
inline std::string encode(const Bytes& bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t value = uint32_t(bytes[i]) << 16 |
            (i + 1 < bytes.size() ? uint32_t(bytes[i + 1]) << 8 : 0) |
            (i + 2 < bytes.size() ? bytes[i + 2] : 0);
        out += alphabet[value >> 18];
        out += alphabet[(value >> 12) & 63];
        out += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return out;
}
inline Bytes decode(const std::string& text) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    Bytes bytes;
    uint32_t value = 0;
    int bits = 0;
    for (char ch : text) {
        if (ch == '=') break;
        const auto digit = alphabet.find(ch);
        require(digit != std::string::npos, "Invalid reference base64");
        value = (value << 6) | uint32_t(digit);
        bits += 6;
        if (bits >= 8) { bits -= 8; bytes.push_back(uint8_t(value >> bits)); }
    }
    return bytes;
}
inline void be32(Bytes& bytes, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(uint8_t(value >> shift));
}
inline uint32_t read32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
inline void chunk(Bytes& out, const std::string& type, const Bytes& payload) {
    be32(out, uint32_t(payload.size()));
    const size_t start = out.size();
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), payload.begin(), payload.end());
    be32(out, uint32_t(crc32(0, out.data() + start, uInt(4 + payload.size()))));
}
inline void text(Bytes& out, const std::string& key, const std::string& value) {
    Bytes payload(key.begin(), key.end());
    payload.push_back(0);
    payload.insert(payload.end(), value.begin(), value.end());
    chunk(out, "tEXt", payload);
}
inline Bytes png(size_t imageBytes = 0) {
    auto out = decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=");
    if (imageBytes) {
        // Chunk-level benchmark: synthetic IDAT avoids image compression changing
        // the desired input size. Pixel decoding is not part of PNG metadata edits.
        out.resize(33);
        Bytes image(imageBytes);
        uint32_t state = 42;
        for (auto& byte : image) { state = state * 1664525 + 1013904223; byte = uint8_t(state >> 24); }
        chunk(out, "IDAT", image);
        chunk(out, "IEND", {});
    }
    return out;
}
inline Bytes withText(const std::string& key, const std::string& value, bool after = false) {
    auto image = png();
    const size_t offset = after ? image.size() - 12 : 33;
    Bytes added;
    text(added, key, value);
    image.insert(image.begin() + offset, added.begin(), added.end());
    return image;
}
inline void validCRCs(const Bytes& png) {
    for (size_t i = 8; i + 12 <= png.size();) {
        const size_t length = read32(png.data() + i);
        require(length <= png.size() - i - 12, "Output chunk exceeds buffer");
        require(read32(png.data() + i + length + 8) ==
            crc32(0, png.data() + i + 4, uInt(length + 4)), "Output CRC mismatch");
        i += length + 12;
    }
}
}
