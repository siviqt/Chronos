#include "platform_pcm.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Read a little-endian RIFF integer without alignment assumptions. */
static uint32_t u32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
/* Read a little-endian PCM format field. */
static uint16_t u16(const uint8_t* p) {
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
/* Free clip ownership and clear metadata together. */
void platform_pcm_free(platform_pcm_t* pcm) {
    free(pcm->data);
    *pcm = (platform_pcm_t){0};
}
/* Parse RIFF chunks including padding, rejecting truncated or inconsistent format/data. */
bool platform_pcm_load(FILE* file, platform_pcm_t* out) {
    *out = (platform_pcm_t){0};
    uint8_t header[12];
    if (fseek(file, 0, SEEK_END)) return false;
    long file_size = ftell(file);
    if (file_size < 12 || file_size > PLATFORM_PCM_MAX_BYTES + 65536L || fseek(file, 0, SEEK_SET)) return false;
    if (fread(header, 1, 12, file) != 12 || memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4)) return false;
    uint64_t end = (uint64_t)u32(header + 4) + 8;
    if (end != (uint64_t)file_size) return false;
    bool     format_found = false, data_found = false;
    uint16_t channels    = 0;
    uint32_t data_length = 0, rate = 0;
    long     data_offset = 0;
    uint64_t position    = 12;
    while (position < end) {
        if (end - position < 8 || fread(header, 1, 8, file) != 8) return false;
        uint32_t length = u32(header + 4);
        uint64_t next   = position + 8 + length + (length & 1u);
        if (next > end || next > LONG_MAX) return false;
        if (!memcmp(header, "fmt ", 4)) {
            uint8_t format[16];
            if (format_found || length < 16 || fread(format, 1, 16, file) != 16) return false;
            channels = u16(format + 2);
            rate = u32(format + 4);
            if (u16(format) != 1 || (channels != 1 && channels != 2) || u16(format + 14) != 16 ||
                (rate < 8000 || rate > 48000) || u16(format + 12) != channels * 2 ||
                u32(format + 8) != rate * channels * 2)
                return false;
            format_found = true;
        } else if (!memcmp(header, "data", 4)) {
            if (data_found || !length || length > PLATFORM_PCM_MAX_BYTES) return false;
            data_offset = (long)(position + 8);
            data_length = length;
            data_found  = true;
        }
        if (fseek(file, (long)next, SEEK_SET)) return false;
        position = next;
    }
    if (!format_found || !data_found || data_length % (channels * 2)) return false;
    uint8_t* data = malloc(data_length);
    if (!data) return false;
    if (fseek(file, data_offset, SEEK_SET) || fread(data, 1, data_length, file) != data_length) {
        free(data);
        return false;
    }
    platform_pcm_t source = {.data = data, .frames = data_length / (channels * 2), .channels = channels};
    if (rate != PLATFORM_AUDIO_RATE) {
        size_t frames = ((uint64_t)source.frames * PLATFORM_AUDIO_RATE + rate - 1) / rate;
        if (frames > PLATFORM_PCM_MAX_BYTES / (channels * 2)) { free(data); return false; }
        uint8_t* converted = malloc(frames * channels * 2);
        if (!converted) { free(data); return false; }
        /* Linear interpolation preserves duration/pitch at the fixed codec rate. */
        for (size_t f = 0; f < frames; ++f) {
            uint64_t phase = (uint64_t)f * rate;
            size_t first = phase / PLATFORM_AUDIO_RATE;
            size_t next = first + 1 < source.frames ? first + 1 : first;
            unsigned fraction = phase % PLATFORM_AUDIO_RATE;
            for (unsigned c = 0; c < channels; ++c) {
                int32_t a = platform_pcm_sample(&source, first, c);
                int32_t b = platform_pcm_sample(&source, next, c);
                int32_t value = a + (int64_t)(b - a) * fraction / PLATFORM_AUDIO_RATE;
                size_t i = (f * channels + c) * 2;
                converted[i] = (uint16_t)value & 255;
                converted[i + 1] = (uint16_t)value >> 8;
            }
        }
        free(data);
        source.data = converted; source.frames = frames;
    }
    *out = source;
    return true;
}
/* Expand mono to either stereo channel and preserve signed PCM values portably. */
int16_t platform_pcm_sample(const platform_pcm_t* pcm, size_t frame, unsigned int channel) {
    if (!pcm->data || frame >= pcm->frames || channel > 1) return 0;
    size_t   index = frame * pcm->channels + (pcm->channels == 1 ? 0 : channel);
    uint16_t raw   = u16(pcm->data + index * 2);
    return (int16_t)(raw <= INT16_MAX ? (int32_t)raw : (int32_t)raw - 65536);
}
/* Generate a bounded, click-reduced fallback without any external asset or allocation. */
int16_t platform_pcm_tone(uint64_t sample, bool pulse) {
    uint32_t cycle    = (uint32_t)(sample % PLATFORM_AUDIO_RATE);
    uint32_t burst    = pulse ? cycle : cycle % (PLATFORM_AUDIO_RATE / 2);
    uint32_t duration = pulse ? PLATFORM_PACER_FRAMES : PLATFORM_AUDIO_RATE / 5;
    if (burst >= duration) return 0;
    uint32_t frequency = pulse ? 1320 : (cycle < PLATFORM_AUDIO_RATE / 2 ? 880 : 1320);
    uint32_t phase     = (uint32_t)((sample % PLATFORM_AUDIO_RATE) * frequency * 1024 / PLATFORM_AUDIO_RATE) % 1024;
    int32_t  value     = phase < 512 ? (int32_t)phase * 2 - 512 : 1536 - (int32_t)phase * 2;
    if (pulse) value = phase < 512 ? 512 : -512;
    uint32_t ramp      = PLATFORM_AUDIO_RATE / 200;
    uint32_t gain      = burst < ramp ? burst : (duration - burst < ramp ? duration - burst : ramp);
    /* Cadence uses near-full-scale PCM; the codec applies configured volume. */
    return (int16_t)(value * (pulse ? 60 : 12) * (int32_t)gain / (int32_t)ramp);
}
