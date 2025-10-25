#include "HybridPngUtils.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace margelo::nitro::pngutils {

static constexpr int8_t BASE64_TABLE[256] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
  52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
  -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
};

static inline bool base64DecodeToBytes(std::string_view in, std::vector<uint8_t>& out) {
  out.clear();
  out.reserve((in.size() / 4) * 3);

  int val = 0;
  int valb = -8;

  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(in.data());
  const unsigned char* end = ptr + in.size();

  while (ptr < end) {
    int8_t d = BASE64_TABLE[*ptr++];
    if (d == -1) continue;  // skip padding or invalid
    val = (val << 6) + d;
    valb += 6;
    if (valb >= 0) {
      out.push_back(uint8_t((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return true;
}

static inline uint32_t readUInt32BE(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) |
         (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8)  |
          uint32_t(p[3]);
}

std::string HybridPngUtils::getPngChunk(const std::string& base64Png) {
  static thread_local std::vector<uint8_t> bytes;
  bytes.clear();

  if (!base64DecodeToBytes(base64Png, bytes))
    throw std::runtime_error("Invalid Base64 PNG data");

  // Validate PNG header
  if (bytes.size() < 8)
    throw std::runtime_error("Invalid PNG: too short");

  static const uint8_t PNG_HEADER[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
  if (std::memcmp(bytes.data(), PNG_HEADER, 8) != 0)
    throw std::runtime_error("Invalid PNG header");

  const uint8_t* p = bytes.data() + 8;
  const uint8_t* end = bytes.data() + bytes.size();

  while (p + 12 <= end) { // length(4) + type(4) + crc(4)
    uint32_t length = readUInt32BE(p); p += 4;
    if (p + 4 > end) break;

    std::string_view typeSv(reinterpret_cast<const char*>(p), 4);
    p += 4;

    if (p + length + 4 > end)
      throw std::runtime_error("Corrupted PNG: chunk exceeds file size");

    std::string_view chunkSv(reinterpret_cast<const char*>(p), length);
    p += length + 4; // skip CRC

    if (typeSv == "tEXt") {
      size_t nullPos = chunkSv.find('\0');
      if (nullPos == std::string_view::npos)
        throw std::runtime_error("Malformed tEXt chunk: missing null separator");

      std::string_view valueSv = chunkSv.substr(nullPos + 1);

      std::vector<uint8_t> tmp;
      tmp.reserve((valueSv.size() / 4) * 3 + 4);

      if (!base64DecodeToBytes(valueSv, tmp))
        throw std::runtime_error("Failed to decode tEXt chunk value");

      std::string result(reinterpret_cast<const char*>(tmp.data()), tmp.size());
      while (!result.empty() && (unsigned char)result.back() <= 0x1F)
        result.pop_back();
      return result;
    }
  }

  throw std::runtime_error("tEXt chunk not found");
}

} // namespace margelo::nitro::pngutils
