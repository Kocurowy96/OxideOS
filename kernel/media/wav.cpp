#include "wav.h"

struct ChunkHeader {
    uint32_t id;
    uint32_t size;
} __attribute__((packed));

struct FmtChunk {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} __attribute__((packed));

static uint32_t FindChunk(uint8_t* buffer, uint32_t target_id) {
    uint32_t offset = 12; // Skip RIFF header
    while (offset < 4096) { // Look in first 4KB for headers
        ChunkHeader* chunk = (ChunkHeader*)(buffer + offset);
        if (chunk->id == target_id) {
            return offset;
        }
        offset += 8 + chunk->size;
    }
    return 0;
}

bool WAV::IsValidWAV(uint8_t* buffer, uint32_t size) {
    if (size < 44) return false;
    uint32_t* chunk_id = (uint32_t*)buffer;
    if (chunk_id[0] != 0x46464952) return false; // "RIFF"
    if (chunk_id[2] != 0x45564157) return false; // "WAVE"
    return true;
}

uint8_t* WAV::GetData(uint8_t* buffer) {
    uint32_t offset = FindChunk(buffer, 0x61746164); // "data"
    if (offset) return buffer + offset + 8;
    return nullptr;
}

uint32_t WAV::GetDataSize(uint8_t* buffer) {
    uint32_t offset = FindChunk(buffer, 0x61746164); // "data"
    if (offset) {
        ChunkHeader* chunk = (ChunkHeader*)(buffer + offset);
        return chunk->size;
    }
    return 0;
}

uint32_t WAV::GetSampleRate(uint8_t* buffer) {
    uint32_t offset = FindChunk(buffer, 0x20746D66); // "fmt "
    if (offset) {
        FmtChunk* fmt = (FmtChunk*)(buffer + offset + 8);
        return fmt->sample_rate;
    }
    return 0;
}

uint16_t WAV::GetChannels(uint8_t* buffer) {
    uint32_t offset = FindChunk(buffer, 0x20746D66); // "fmt "
    if (offset) {
        FmtChunk* fmt = (FmtChunk*)(buffer + offset + 8);
        return fmt->num_channels;
    }
    return 0;
}

uint16_t WAV::GetBitsPerSample(uint8_t* buffer) {
    uint32_t offset = FindChunk(buffer, 0x20746D66); // "fmt "
    if (offset) {
        FmtChunk* fmt = (FmtChunk*)(buffer + offset + 8);
        return fmt->bits_per_sample;
    }
    return 0;
}
