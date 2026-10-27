#include "AudioDecoder.hpp"
#include <iostream>
#include <exception>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto result = margelo::nitro::pngutils::audio::decodeToWav(argv[1], argv[2]);
        std::cout << result.sampleRate << ' ' << result.channels << ' ' << result.frames << ' ' << result.bytes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
