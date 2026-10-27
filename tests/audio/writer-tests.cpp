#include "WavWriter.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
using margelo::nitro::pngutils::audio::WavWriter;
namespace fs = std::filesystem;
void require(bool condition) { if (!condition) throw std::runtime_error("WAV writer test failed"); }
template <class Run> void rejects(Run run) {
    try { run(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Expected WAV writer error");
}
int main() {
    char pattern[] = "/tmp/png-wav-writer-XXXXXX";
    const char* directory = mkdtemp(pattern);
    require(directory != nullptr);
    const fs::path root(directory);
    struct Cleanup { fs::path path; ~Cleanup() { fs::remove_all(path); } } cleanup{root};
    const auto destination = (root / "output.wav").string();
    rejects([&] { WavWriter writer(destination, 48000, 2); writer.finish(); });
    require(fs::is_empty(root));
    rejects([&] { WavWriter writer(destination, 48000, 2); writer.write("x", 1); });
    require(fs::is_empty(root));
    rejects([&] { WavWriter writer(destination, 48000, 2); writer.write("x", UINT32_MAX - 3ULL); });
    require(fs::is_empty(root));
    {
        WavWriter first(destination, 48000, 2);
        rejects([&] { WavWriter second(destination, 48000, 2); });
        const std::array<int16_t, 4> pcm{0, 100, -100, 32767};
        first.write(pcm.data(), sizeof(pcm));
        const auto result = first.finish();
        require(result.frames == 2 && result.bytes == 52);
        require(fs::file_size(destination) == 52);
    }
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(root)) { (void)entry; ++count; }
    require(count == 1);
    rejects([&] { WavWriter writer(destination, 48000, 2); });
    std::cout << "WAV writer cleanup, frame validation, size limit and exclusive no-overwrite tests passed\n";
}
