#pragma once
#include <stdint.h>

struct AC97BufferDescriptor {
    uint32_t buffer_addr;
    uint16_t length;
    uint16_t flags;
} __attribute__((packed));

class AC97 {
public:
    static void Init();
    static void PlayWAV(uint8_t* wav_data);
    static void PlaySquareWave();
    static void WriteCodec(uint8_t reg, uint16_t value);
    static uint16_t ReadCodec(uint8_t reg);
};
