#include "fixtures.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <sys/resource.h>
using namespace margelo::nitro::pngutils;
int main(int argc, char** argv) {
    const size_t mib = argc > 1 ? std::stoul(argv[1]) : 8;
    const size_t iterations = argc > 2 ? std::stoul(argv[2]) : 20;
    const std::string operation = argc > 3 ? argv[3] : "replace";
    std::string input;
    {
        fixture::Bytes bytes;
        if (operation == "convert") {
            const std::string header = "P6\n1024 1024\n255\n";
            bytes.assign(header.begin(), header.end());
            uint32_t state = 42;
            for (size_t i = 0; i < 1024 * 1024 * 3; ++i) {
                state = state * 1664525 + 1013904223;
                bytes.push_back(uint8_t(state >> 24));
            }
        } else {
            bytes = fixture::png(mib * 1024 * 1024);
            fixture::Bytes metadata;
            fixture::text(metadata, "chara", "eyJuYW1lIjoiVGVzdCJ9");
            bytes.insert(bytes.begin() + 33, metadata.begin(), metadata.end());
        }
        input = fixture::encode(bytes);
    }
    HybridPngUtils utils;
    const std::vector<TextChunk> update{{"chara", "{\"name\":\"Updated\"}", true}};
    const auto start = std::chrono::steady_clock::now();
    const auto cpuStart = std::clock();
    size_t checksum = 0;
    for (size_t i = 0; i < iterations; ++i) {
        if (operation == "extract") checksum += utils.extractPngChunks(input, {})[0].data.size();
        else checksum += utils.replacePngChunks(input, update, {}).size();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    std::cout << operation << " input_MiB=" << mib << " iterations=" << iterations
              << " ms_per_call=" << ms / iterations
              << " cpu_ms_per_call=" << 1000.0 * (std::clock() - cpuStart) / CLOCKS_PER_SEC / iterations
              << " maxrss=" << usage.ru_maxrss
              << " checksum=" << checksum << '\n';
}
