#include "WavWriter.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
namespace margelo::nitro::pngutils::audio {
namespace {
void le16(uint8_t* p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
void le32(uint8_t* p, uint32_t n) { for (int i = 0; i < 4; ++i) p[i] = n >> (i * 8); }
std::runtime_error ioError(const char* operation) {
    return std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
}
WavWriter::WavWriter(const std::string& output, uint32_t rate, uint16_t channels)
    : output_(output), rate_(rate), channels_(channels) {
    if (output.empty() || output[0] != '/' || output.find('\0') != std::string::npos)
        throw std::runtime_error("Output must be an absolute local filesystem path");
    if (!rate || channels < 1 || channels > 2 || uint64_t(rate) * channels * 2 > UINT32_MAX)
        throw std::runtime_error("Only mono/stereo PCM audio is supported");
    // App-private storage may prohibit hard links. Create the requested file
    // directly and exclusively; remove it on failure until finish() succeeds.
    const int fd = open(output.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) throw ioError("Create WAV output");
    pending_ = output;
    file_ = fdopen(fd, "w+b");
    if (!file_) { close(fd); unlink(pending_.c_str()); throw ioError("Open WAV stream"); }
    const std::array<uint8_t, 44> placeholder{};
    if (fwrite(placeholder.data(), 1, placeholder.size(), file_) != placeholder.size()) {
        fclose(file_); file_ = nullptr; unlink(pending_.c_str());
        throw ioError("Write WAV header");
    }
}
WavWriter::~WavWriter() {
    if (file_) fclose(file_);
    if (!pending_.empty()) unlink(pending_.c_str());
}
void WavWriter::write(const void* pcm, size_t count) {
    if (!file_) throw std::runtime_error("WAV stream is closed");
    if (count % (channels_ * 2)) throw std::runtime_error("Incomplete PCM frame");
    if (count > UINT32_MAX - 36 - bytes_) throw std::runtime_error("Audio exceeds the classic WAV 4 GiB limit");
    if (count && fwrite(pcm, 1, count, file_) != count) throw ioError("Write WAV audio");
    bytes_ += count;
}
Conversion WavWriter::finish() {
    if (!file_ || !bytes_) throw std::runtime_error("No decoded audio frames");
    std::array<uint8_t, 44> header{};
    std::memcpy(header.data(), "RIFF", 4);
    le32(header.data() + 4, static_cast<uint32_t>(36 + bytes_));
    std::memcpy(header.data() + 8, "WAVEfmt ", 8);
    le32(header.data() + 16, 16);
    le16(header.data() + 20, 1);
    le16(header.data() + 22, channels_);
    le32(header.data() + 24, rate_);
    le32(header.data() + 28, rate_ * channels_ * 2);
    le16(header.data() + 32, channels_ * 2);
    le16(header.data() + 34, 16);
    std::memcpy(header.data() + 36, "data", 4);
    le32(header.data() + 40, static_cast<uint32_t>(bytes_));
    if (fseek(file_, 0, SEEK_SET) || fwrite(header.data(), 1, header.size(), file_) != header.size())
        throw ioError("Finalize WAV header");
    FILE* closing = file_; file_ = nullptr;
    if (fclose(closing)) throw ioError("Close WAV file");
    pending_.clear(); // The completed file now belongs to the caller.
    return {rate_, channels_, bytes_ / (channels_ * 2), bytes_ + 44};
}
}
