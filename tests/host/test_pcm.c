#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform_pcm.h"

/* Write a little-endian integer for independent WAV fixtures. */
static void le32(uint8_t* bytes, uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
}
/* Build a tiny RIFF PCM fixture containing the signed 16-bit endpoints. */
static size_t fixture(uint8_t* bytes, bool stereo) {
    memset(bytes, 0, 64);
    memcpy(bytes, "RIFF", 4);
    le32(bytes + 4, 40);
    memcpy(bytes + 8, "WAVEfmt ", 8);
    le32(bytes + 16, 16);
    bytes[20] = 1;
    bytes[22] = stereo ? 2 : 1;
    le32(bytes + 24, 44100);
    le32(bytes + 28, stereo ? 176400 : 88200);
    bytes[32] = stereo ? 4 : 2;
    bytes[34] = 16;
    memcpy(bytes + 36, "data", 4);
    le32(bytes + 40, 4);
    bytes[44] = 0xff;
    bytes[45] = 0x7f;
    bytes[46] = 0;
    bytes[47] = 0x80;
    return 48;
}
/* Exercise the production FILE parser without any SD dependencies. */
static bool load_bytes(uint8_t* bytes, size_t length, platform_pcm_t* pcm) {
    FILE* file = tmpfile();
    assert(file);
    assert(fwrite(bytes, 1, length, file) == length);
    bool result = platform_pcm_load(file, pcm);
    assert(fclose(file) == 0);
    return result;
}
/* Test decoding, truncation, invalid formats, unknown chunks, and malformed sizes. */
static void test_wave(void) {
    uint8_t        bytes[128], original[128];
    platform_pcm_t pcm    = {0};
    size_t         length = fixture(bytes, false);
    assert(load_bytes(bytes, length, &pcm));
    assert(pcm.frames == 2 && pcm.channels == 1);
    assert(platform_pcm_sample(&pcm, 0, 0) == 32767);
    assert(platform_pcm_sample(&pcm, 1, 1) == -32768);
    assert(platform_pcm_sample(&pcm, 2, 0) == 0);
    platform_pcm_free(&pcm);
    length = fixture(bytes, true);
    assert(load_bytes(bytes, length, &pcm));
    assert(pcm.frames == 1 && platform_pcm_sample(&pcm, 0, 1) == -32768);
    platform_pcm_free(&pcm);
    memcpy(original, bytes, length);
    for (size_t i = 0; i < length; ++i) {
        assert(!load_bytes(bytes, i, &pcm));
        assert(!pcm.data);
    }
    const size_t corruptions[] = {0, 4, 8, 12, 16, 20, 22, 24, 28, 32, 34, 36, 40};
    for (size_t i = 0; i < sizeof(corruptions) / sizeof(corruptions[0]); ++i) {
        memcpy(bytes, original, length);
        bytes[corruptions[i]] ^= 0x80;
        assert(!load_bytes(bytes, length, &pcm));
    }
    /* Skip an odd-length metadata chunk and its RIFF padding. */
    memcpy(bytes, original, 36);
    memcpy(bytes + 36, "JUNK", 4);
    le32(bytes + 40, 1);
    bytes[44] = 7;
    bytes[45] = 0;
    memcpy(bytes + 46, original + 36, 12);
    le32(bytes + 4, 50);
    assert(load_bytes(bytes, 58, &pcm));
    platform_pcm_free(&pcm);
    le32(bytes + 40, UINT32_MAX);
    assert(!load_bytes(bytes, 58, &pcm));
    /* Data may precede the fmt chunk; parsing remains bounded and deterministic. */
    memcpy(bytes, original, 12);
    memcpy(bytes + 12, original + 36, 12);
    memcpy(bytes + 24, original + 12, 24);
    assert(load_bytes(bytes, 48, &pcm));
    platform_pcm_free(&pcm);
}
/* Confirm fallback tone is nonempty, bounded, burst-shaped, and silent after a pacer pulse. */
static void test_tone(void) {
    bool nonzero = false;
    for (uint64_t i = 0; i < 44100; ++i) {
        int value = platform_pcm_tone(i, false);
        assert(value >= -6144 && value <= 6144);
        nonzero |= value != 0;
        if (i >= PLATFORM_PACER_FRAMES) assert(platform_pcm_tone(i, true) == 0);
    }
    int peak = 0;
    for (unsigned i = 0; i < PLATFORM_PACER_FRAMES; ++i) {
        int pulse = platform_pcm_tone(i, true);
        assert(pulse >= -30720 && pulse <= 30720);
        if (pulse > peak) peak = pulse;
    }
    assert(peak > 29000);
    assert(nonzero && platform_pcm_tone(0, false) == 0);
    assert(platform_pcm_tone(16000, false) == 0);
}
/* Run the audio format and generated-sound suite. */
int main(void) {
    test_wave();
    test_tone();
    const unsigned rates[] = {8000, 11025, 31250, 32000, 44100, 48000};
    for (unsigned i = 0; i < sizeof(rates)/sizeof(rates[0]); ++i) {
        uint8_t bytes[64]; platform_pcm_t pcm = {0};
        size_t n = fixture(bytes, false);
        le32(bytes + 24, rates[i]); le32(bytes + 28, rates[i] * 2);
        assert(load_bytes(bytes, n, &pcm));
        assert(pcm.frames == (2u * 44100 + rates[i] - 1) / rates[i]);
        assert(platform_pcm_sample(&pcm, 0, 0) == 32767);
        platform_pcm_free(&pcm);
    }
    /* Optional user-provided assets are not required for a clean checkout. */
    const char* assets[] = {"TF022.WAV", "TF023.WAV", "TF034.WAV", "TF048.WAV", "TF050.WAV"};
    for (unsigned i = 0; i < 5; ++i) {
        char path[128]; snprintf(path, sizeof(path), "assets/sounds/%s", assets[i]);
        FILE* file = fopen(path, "rb");
        if (!file) continue;
        platform_pcm_t pcm = {0}; assert(platform_pcm_load(file, &pcm));
        assert(pcm.frames && pcm.channels == 1); fclose(file); platform_pcm_free(&pcm);
        printf("Hardware WAV decoded: %s\n", path);
    }
    puts("PCM: all tests passed");
    return 0;
}
