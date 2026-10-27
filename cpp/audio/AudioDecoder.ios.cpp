#if defined(__APPLE__)
#include "AudioDecoder.hpp"
#include "WavWriter.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <cmath>
#include <stdexcept>
namespace margelo::nitro::pngutils::audio {
namespace {
void check(OSStatus status, const char* operation) {
    if (status != noErr) throw std::runtime_error(std::string(operation) + " (OSStatus " + std::to_string(status) + ")");
}
struct Input {
    ExtAudioFileRef file = nullptr;
    ~Input() { if (file) ExtAudioFileDispose(file); }
};
}
Conversion decodeToWav(const std::string& input, const std::string& output) {
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(input.data()), input.size(), false);
    if (!url) throw std::runtime_error("Invalid input path");
    Input source;
    const OSStatus opened = ExtAudioFileOpenURL(url, &source.file);
    CFRelease(url);
    check(opened, "Open audio input");
    AudioStreamBasicDescription original{};
    UInt32 size = sizeof(original);
    check(ExtAudioFileGetProperty(source.file, kExtAudioFileProperty_FileDataFormat, &size, &original), "Read audio format");
    if (original.mChannelsPerFrame < 1 || original.mChannelsPerFrame > 2 ||
        !std::isfinite(original.mSampleRate) || original.mSampleRate < 1 || original.mSampleRate > 768000 ||
        std::floor(original.mSampleRate) != original.mSampleRate)
        throw std::runtime_error("Unsupported audio format: expected mono/stereo with an integer sample rate");
    AudioStreamBasicDescription pcm{};
    pcm.mSampleRate = original.mSampleRate;
    pcm.mFormatID = kAudioFormatLinearPCM;
    pcm.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    pcm.mChannelsPerFrame = original.mChannelsPerFrame;
    pcm.mBitsPerChannel = 16;
    pcm.mFramesPerPacket = 1;
    pcm.mBytesPerFrame = pcm.mChannelsPerFrame * 2;
    pcm.mBytesPerPacket = pcm.mBytesPerFrame;
    check(ExtAudioFileSetProperty(source.file, kExtAudioFileProperty_ClientDataFormat, sizeof(pcm), &pcm), "Configure PCM conversion");
    WavWriter writer(output, static_cast<uint32_t>(pcm.mSampleRate), static_cast<uint16_t>(pcm.mChannelsPerFrame));
    std::array<uint8_t, 32768> buffer{};
    for (;;) {
        UInt32 frames = buffer.size() / pcm.mBytesPerFrame;
        AudioBufferList list{};
        list.mNumberBuffers = 1;
        list.mBuffers[0] = {pcm.mChannelsPerFrame, static_cast<UInt32>(buffer.size()), buffer.data()};
        check(ExtAudioFileRead(source.file, &frames, &list), "Decode audio");
        if (!frames) break;
        writer.write(buffer.data(), frames * pcm.mBytesPerFrame);
    }
    return writer.finish();
}
}
#endif
