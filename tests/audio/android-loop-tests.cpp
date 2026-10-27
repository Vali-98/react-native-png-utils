#include "AudioDecoder.hpp"
#include "media/FakeMedia.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace fs = std::filesystem;
void require(bool ok) { if (!ok) throw std::runtime_error("Android input-loop regression"); }
int main() {
    char pattern[] = "/tmp/png-android-loop-XXXXXX";
    const char* directory = mkdtemp(pattern);
    require(directory != nullptr);
    const fs::path root(directory);
    struct Cleanup { fs::path path; ~Cleanup() { fs::remove_all(path); } } cleanup{root};
    const auto input = (root / "input.m4a").string();
    std::ofstream(input) << "fake encoded audio";
    using margelo::nitro::pngutils::audio::decodeToWav;
    // Negative priming PTS, including -1, must be fed to the decoder, not EOS.
    fake::reset({-64000, -1, 0, 64000});
    const auto output = (root / "output.wav").string();
    const auto result = decodeToWav(input, output);
    require(result.frames == 16 && result.bytes == 76);
    require(fake::reads == 4 && fake::submitted.size() == 4 && fake::flagsAtEnd == 0);
    // A real encrypted sample must still reject; EOS sentinel flags must not.
    fake::reset({0}, true);
    try { decodeToWav(input, (root / "encrypted.wav").string()); require(false); }
    catch (const std::runtime_error& error) { require(std::string(error.what()).find("Encrypted") != std::string::npos); }
    fake::reset({0}, false, true);
    try { decodeToWav(input, (root / "broken.wav").string()); require(false); }
    catch (const std::runtime_error& error) { require(std::string(error.what()).find("Cannot read") != std::string::npos); }
    require(!fs::exists(root / "encrypted.wav") && !fs::exists(root / "broken.wav"));
    std::cout << "Android production loop: negative AAC timestamps, EOS flags, encrypted samples and read failures passed\n";
}
