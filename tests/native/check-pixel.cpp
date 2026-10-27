#include "fixtures.hpp"
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
void checkPixel(const fixture::Bytes& bytes) {
    int w, h, channels;
    auto* pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
    fixture::require(pixels && w == 1 && h == 1, "Conversion output cannot be decoded");
    const bool match = pixels[0] == 0x11 && pixels[1] == 0x22 && pixels[2] == 0x33 && pixels[3] == 255;
    stbi_image_free(pixels);
    fixture::require(match, "Conversion changed pixels");
}
