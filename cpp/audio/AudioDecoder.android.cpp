#if defined(__ANDROID__) || defined(PNGUTIL_TEST_ANDROID_DECODER)
#include "AudioDecoder.hpp"
#include "WavWriter.hpp"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace margelo::nitro::pngutils::audio {
namespace {
void check(media_status_t status, const char* operation) {
    if (status != AMEDIA_OK) throw std::runtime_error(std::string(operation) + " (media status " + std::to_string(status) + ")");
}
struct InputFd { int fd; ~InputFd() { if (fd >= 0) close(fd); } };
struct Codec {
    AMediaCodec* value = nullptr;
    bool started = false;
    ~Codec() { if (value) { if (started) AMediaCodec_stop(value); AMediaCodec_delete(value); } }
};
using Format = std::unique_ptr<AMediaFormat, decltype(&AMediaFormat_delete)>;
}
Conversion decodeToWav(const std::string& input, const std::string& output) {
    InputFd file{open(input.c_str(), O_RDONLY | O_CLOEXEC)};
    struct stat stat{};
    if (file.fd < 0 || fstat(file.fd, &stat) || !S_ISREG(stat.st_mode) || stat.st_size == 0)
        throw std::runtime_error("Cannot open a nonempty local audio file");
    std::unique_ptr<AMediaExtractor, decltype(&AMediaExtractor_delete)> extractor(AMediaExtractor_new(), AMediaExtractor_delete);
    if (!extractor) throw std::runtime_error("Cannot create media extractor");
    check(AMediaExtractor_setDataSourceFd(extractor.get(), file.fd, 0, stat.st_size), "Open media container");
    Format track(nullptr, AMediaFormat_delete);
    for (size_t index = 0; index < AMediaExtractor_getTrackCount(extractor.get()); ++index) {
        Format candidate(AMediaExtractor_getTrackFormat(extractor.get(), index), AMediaFormat_delete);
        const char* mime = nullptr;
        if (candidate && AMediaFormat_getString(candidate.get(), AMEDIAFORMAT_KEY_MIME, &mime) &&
            mime && std::strncmp(mime, "audio/", 6) == 0) {
            check(AMediaExtractor_selectTrack(extractor.get(), index), "Select audio track");
            track = std::move(candidate);
            break;
        }
    }
    if (!track) throw std::runtime_error("Input contains no audio track");
    const char* mime = nullptr;
    AMediaFormat_getString(track.get(), AMEDIAFORMAT_KEY_MIME, &mime);
    Codec codec;
    codec.value = AMediaCodec_createDecoderByType(mime);
    if (!codec.value) throw std::runtime_error("No platform decoder for this audio codec");
    AMediaFormat_setInt32(track.get(), "pcm-encoding", 2); // ENCODING_PCM_16BIT
    check(AMediaCodec_configure(codec.value, track.get(), nullptr, nullptr, 0), "Configure audio decoder");
    check(AMediaCodec_start(codec.value), "Start audio decoder");
    codec.started = true;
    std::unique_ptr<WavWriter> writer;
    int32_t rate = 0, channels = 0, encoding = 2;
    auto configureOutput = [&]() {
        Format format(AMediaCodec_getOutputFormat(codec.value), AMediaFormat_delete);
        int32_t nextRate = 0, nextChannels = 0, nextEncoding = 2;
        if (!format || !AMediaFormat_getInt32(format.get(), AMEDIAFORMAT_KEY_SAMPLE_RATE, &nextRate) ||
            !AMediaFormat_getInt32(format.get(), AMEDIAFORMAT_KEY_CHANNEL_COUNT, &nextChannels) ||
            nextRate <= 0 || nextChannels < 1 || nextChannels > 2)
            throw std::runtime_error("Unsupported decoder format: expected mono/stereo PCM");
        AMediaFormat_getInt32(format.get(), "pcm-encoding", &nextEncoding);
        if (nextEncoding != 2 && nextEncoding != 4) throw std::runtime_error("Decoder returned unsupported PCM encoding");
        if (writer && (nextRate != rate || nextChannels != channels || nextEncoding != encoding))
            throw std::runtime_error("Audio format changed during conversion");
        rate = nextRate; channels = nextChannels; encoding = nextEncoding;
        if (!writer) writer = std::make_unique<WavWriter>(output, rate, static_cast<uint16_t>(channels));
    };
    bool inputEnded = false, outputEnded = false;
    auto progress = std::chrono::steady_clock::now();
    std::array<int16_t, 8192> converted{};
    while (!outputEnded) {
        if (!inputEnded) {
            const ssize_t index = AMediaCodec_dequeueInputBuffer(codec.value, 10000);
            if (index >= 0) {
                size_t capacity = 0;
                auto* buffer = AMediaCodec_getInputBuffer(codec.value, index, &capacity);
                if (!buffer) throw std::runtime_error("Missing decoder input buffer");
                // AAC encoder priming can have negative presentation timestamps.
                // Only the track index determines EOS. Flags at EOS may be -1
                // cast to uint32_t, which would falsely appear encrypted.
                inputEnded = AMediaExtractor_getSampleTrackIndex(extractor.get()) < 0;
                int64_t sampleTime = 0;
                ssize_t bytes = 0;
                if (!inputEnded) {
                    if (AMediaExtractor_getSampleFlags(extractor.get()) & AMEDIAEXTRACTOR_SAMPLE_FLAG_ENCRYPTED)
                        throw std::runtime_error("Encrypted audio is not supported");
                    sampleTime = AMediaExtractor_getSampleTime(extractor.get());
                    bytes = AMediaExtractor_readSampleData(extractor.get(), buffer, capacity);
                    if (bytes < 0) throw std::runtime_error("Cannot read encoded audio packet");
                    if (bytes > static_cast<ssize_t>(capacity)) throw std::runtime_error("Encoded audio packet exceeds decoder buffer");
                }
                const int64_t time = inputEnded ? 0 : std::max<int64_t>(0, sampleTime);
                check(AMediaCodec_queueInputBuffer(codec.value, index, 0, inputEnded ? 0 : bytes, time,
                    inputEnded ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0), "Queue audio packet");
                if (!inputEnded) AMediaExtractor_advance(extractor.get());
                progress = std::chrono::steady_clock::now();
            } else if (index != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                throw std::runtime_error("Audio decoder input failure");
            }
        }
        AMediaCodecBufferInfo info{};
        const ssize_t index = AMediaCodec_dequeueOutputBuffer(codec.value, &info, 10000);
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            configureOutput();
            progress = std::chrono::steady_clock::now();
        } else if (index >= 0) {
            // Release even if validation or writing throws.
            struct Release {
                AMediaCodec* codec; size_t index;
                ~Release() { AMediaCodec_releaseOutputBuffer(codec, index, false); }
            } release{codec.value, static_cast<size_t>(index)};
            if (info.size < 0 || info.offset < 0) throw std::runtime_error("Invalid decoded buffer bounds");
            if (info.size && !(info.flags & (AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG | AMEDIACODEC_BUFFER_FLAG_DECODE_ONLY))) {
                if (!writer) configureOutput();
                size_t capacity = 0;
                const auto* data = AMediaCodec_getOutputBuffer(codec.value, index, &capacity);
                if (!data || static_cast<size_t>(info.offset) > capacity ||
                    static_cast<size_t>(info.size) > capacity - info.offset)
                    throw std::runtime_error("Invalid decoded audio buffer");
                data += info.offset;
                if (encoding == 2) writer->write(data, info.size);
                else {
                    if (info.size % (channels * sizeof(float))) throw std::runtime_error("Incomplete float PCM frame");
                    const size_t samples = info.size / sizeof(float);
                    for (size_t offset = 0; offset < samples;) {
                        const size_t count = std::min(samples - offset, converted.size());
                        for (size_t i = 0; i < count; ++i) {
                            float value;
                            std::memcpy(&value, data + (offset + i) * sizeof(float), sizeof(float));
                            value = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0;
                            converted[i] = static_cast<int16_t>(value < 0 ? value * 32768 : value * 32767);
                        }
                        writer->write(converted.data(), count * sizeof(int16_t));
                        offset += count;
                    }
                }
            }
            outputEnded = info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
            progress = std::chrono::steady_clock::now();
        } else if (index != AMEDIACODEC_INFO_TRY_AGAIN_LATER && index != AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            throw std::runtime_error("Audio decoder output failure");
        }
        if (std::chrono::steady_clock::now() - progress > std::chrono::seconds(30))
            throw std::runtime_error("Audio decoder stalled for 30 seconds");
    }
    if (!writer) throw std::runtime_error("No decoded audio");
    return writer->finish();
}
}
#endif
