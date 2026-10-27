#pragma once
// Host fake for the NDK boundary. The real production Android loop is compiled
// unchanged against this API to exercise packet ordering and EOS handling.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <vector>
using media_status_t = int;
constexpr int AMEDIA_OK = 0;
constexpr int AMEDIACODEC_INFO_TRY_AGAIN_LATER = -1;
constexpr int AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED = -2;
constexpr int AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED = -3;
constexpr uint32_t AMEDIAEXTRACTOR_SAMPLE_FLAG_ENCRYPTED = 2;
constexpr uint32_t AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG = 2;
constexpr uint32_t AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM = 4;
constexpr uint32_t AMEDIACODEC_BUFFER_FLAG_DECODE_ONLY = 32;
constexpr auto AMEDIAFORMAT_KEY_MIME = "mime";
constexpr auto AMEDIAFORMAT_KEY_SAMPLE_RATE = "sample-rate";
constexpr auto AMEDIAFORMAT_KEY_CHANNEL_COUNT = "channel-count";
namespace fake {
inline std::vector<int64_t> timestamps;
inline size_t packet = 0;
inline int flagsAtEnd = 0, reads = 0;
inline bool encrypted = false, readError = false;
inline std::vector<uint64_t> submitted;
inline void reset(std::vector<int64_t> times, bool encryption = false, bool error = false) {
    timestamps = std::move(times); packet = 0; flagsAtEnd = reads = 0;
    encrypted = encryption; readError = error; submitted.clear();
}
}
struct AMediaExtractor {};
struct AMediaFormat {};
struct AMediaCodec {
    bool formatSent = false, inputEnded = false;
    size_t queued = 0, emitted = 0;
    uint8_t input[1024]{};
    int16_t pcm[4]{123, -123, 0, 32767};
};
struct AMediaCodecBufferInfo { int32_t offset = 0, size = 0; int64_t presentationTimeUs = 0; uint32_t flags = 0; };
inline AMediaExtractor* AMediaExtractor_new() { return new AMediaExtractor; }
inline media_status_t AMediaExtractor_delete(AMediaExtractor* p) { delete p; return 0; }
inline media_status_t AMediaExtractor_setDataSourceFd(AMediaExtractor*, int, off_t, off_t) { return 0; }
inline size_t AMediaExtractor_getTrackCount(AMediaExtractor*) { return 1; }
inline AMediaFormat* AMediaExtractor_getTrackFormat(AMediaExtractor*, size_t) { return new AMediaFormat; }
inline media_status_t AMediaExtractor_selectTrack(AMediaExtractor*, size_t) { return 0; }
inline int AMediaExtractor_getSampleTrackIndex(AMediaExtractor*) { return fake::packet < fake::timestamps.size() ? 0 : -1; }
inline int64_t AMediaExtractor_getSampleTime(AMediaExtractor*) { return fake::packet < fake::timestamps.size() ? fake::timestamps[fake::packet] : -1; }
inline uint32_t AMediaExtractor_getSampleFlags(AMediaExtractor*) {
    if (fake::packet == fake::timestamps.size()) { ++fake::flagsAtEnd; return UINT32_MAX; }
    return fake::encrypted ? AMEDIAEXTRACTOR_SAMPLE_FLAG_ENCRYPTED : 0;
}
inline ssize_t AMediaExtractor_readSampleData(AMediaExtractor*, uint8_t* data, size_t) {
    ++fake::reads;
    if (fake::readError) return -1;
    data[0] = 42; return 1;
}
inline bool AMediaExtractor_advance(AMediaExtractor*) { return ++fake::packet < fake::timestamps.size(); }
inline media_status_t AMediaFormat_delete(AMediaFormat* p) { delete p; return 0; }
inline bool AMediaFormat_getString(AMediaFormat*, const char*, const char** value) { *value = "audio/mp4a-latm"; return true; }
inline bool AMediaFormat_getInt32(AMediaFormat*, const char* key, int32_t* value) {
    *value = std::strcmp(key, "sample-rate") == 0 ? 16000 : std::strcmp(key, "channel-count") == 0 ? 1 : 2;
    return true;
}
inline void AMediaFormat_setInt32(AMediaFormat*, const char*, int32_t) {}
inline AMediaCodec* AMediaCodec_createDecoderByType(const char*) { return new AMediaCodec; }
inline media_status_t AMediaCodec_delete(AMediaCodec* p) { delete p; return 0; }
inline media_status_t AMediaCodec_stop(AMediaCodec*) { return 0; }
inline media_status_t AMediaCodec_start(AMediaCodec*) { return 0; }
inline media_status_t AMediaCodec_configure(AMediaCodec*, AMediaFormat*, void*, void*, uint32_t) { return 0; }
inline AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec*) { return new AMediaFormat; }
inline ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec*, int64_t) { return 0; }
inline uint8_t* AMediaCodec_getInputBuffer(AMediaCodec* p, size_t, size_t* capacity) { *capacity = sizeof(p->input); return p->input; }
inline media_status_t AMediaCodec_queueInputBuffer(AMediaCodec* p, size_t, off_t, size_t, uint64_t time, uint32_t flags) {
    if (flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) p->inputEnded = true;
    else { ++p->queued; fake::submitted.push_back(time); }
    return 0;
}
inline ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec* p, AMediaCodecBufferInfo* info, int64_t) {
    if (!p->formatSent) { p->formatSent = true; return AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED; }
    *info = {};
    if (p->emitted < p->queued) { ++p->emitted; info->size = sizeof(p->pcm); return 0; }
    if (p->inputEnded) { info->flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM; return 0; }
    return AMEDIACODEC_INFO_TRY_AGAIN_LATER;
}
inline uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec* p, size_t, size_t* capacity) { *capacity = sizeof(p->pcm); return reinterpret_cast<uint8_t*>(p->pcm); }
inline media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec*, size_t, bool) { return 0; }
