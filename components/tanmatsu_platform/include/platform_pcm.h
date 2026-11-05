#ifndef PLATFORM_PCM_H
#define PLATFORM_PCM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define PLATFORM_AUDIO_RATE    44100
#define PLATFORM_PACER_FRAMES  (PLATFORM_AUDIO_RATE / 10)
#define PLATFORM_PCM_MAX_BYTES (2u * 1024u * 1024u)
typedef struct {
    uint8_t* data;
    size_t   frames;
    uint16_t channels;
} platform_pcm_t;
/* Load bounded PCM WAV (8–48 kHz converted to 44.1 kHz, signed 16-bit, mono/stereo). Initialize out to
 * zero; failure leaves it empty. Release an existing clip before reusing it. */
bool    platform_pcm_load(FILE* file, platform_pcm_t* out);
void    platform_pcm_free(platform_pcm_t* pcm);
/* Decode one channel from a validated clip; out-of-range reads produce silence. */
int16_t platform_pcm_sample(const platform_pcm_t* pcm, size_t frame, unsigned int channel);
/* Ramped square pacer pulse; alternating triangle bursts for fallback alerts. */
int16_t platform_pcm_tone(uint64_t sample, bool pulse);
#endif
