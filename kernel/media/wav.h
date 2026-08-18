#pragma once
#include <stdint.h>

struct WAVHeader {
    uint32_t chunk_id;       // "RIFF" 0x46464952
    uint32_t chunk_size;
    uint32_t format;         // "WAVE" 0x45564157
    uint32_t subchunk1_id;   // "fmt " 0x20746D66
    uint32_t subchunk1_size; // 16 for PCM
    uint16_t audio_format;   // 1 for PCM
    uint16_t num_channels;   // 1 Mono, 2 Stereo
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t subchunk2_id;   // "data" 0x61746164
    uint32_t subchunk2_size; // Number of bytes of data
} __attribute__((packed));

class WAV {
public:
    static bool IsValidWAV(uint8_t* buffer, uint32_t size);
    static uint8_t* GetData(uint8_t* buffer);
    static uint32_t GetDataSize(uint8_t* buffer);
    static uint32_t GetSampleRate(uint8_t* buffer);
    static uint16_t GetChannels(uint8_t* buffer);
    static uint16_t GetBitsPerSample(uint8_t* buffer);
};
