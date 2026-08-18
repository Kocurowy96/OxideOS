#include "ac97.h"
#include "pci.h"
#include "../serial.h"
#include "../cpu/io.h"
#include "../mem/pmm.h"
#include "../media/wav.h"
#include "limine.h"

extern volatile struct limine_hhdm_request hhdm_request;

uint16_t nam_bar = 0;
uint16_t nabm_bar = 0;
AC97BufferDescriptor* bdl = nullptr;

void AC97::WriteCodec(uint8_t reg, uint16_t value) {
    if (!nam_bar) return;
    outw(nam_bar + reg, value);
}

uint16_t AC97::ReadCodec(uint8_t reg) {
    if (!nam_bar) return 0;
    return inw(nam_bar + reg);
}

void AC97::Init() {
    uint8_t bus, slot, func;
    // Intel 82801AA AC97 Audio Controller
    if (PCI::FindDevice(0x8086, 0x2415, &bus, &slot, &func)) {
        SerialPort::WriteString("AC97: Found Intel 82801AA Audio Controller!\n");
        
        nam_bar = PCI::ConfigRead32(bus, slot, func, 0x10) & 0xFFFE;
        nabm_bar = PCI::ConfigRead32(bus, slot, func, 0x14) & 0xFFFE;
        
        // Enable Bus Mastering and I/O Space in Command Register
        uint16_t cmd = PCI::ConfigRead16(bus, slot, func, 0x04);
        cmd |= 0x05; // Bit 0: I/O Space, Bit 2: Bus Master
        PCI::ConfigWrite16(bus, slot, func, 0x04, cmd);
        
        // Global Control Reset (OR instead of overwrite)
        uint32_t glob_ctrl = inl(nabm_bar + 0x2C);
        glob_ctrl |= 2; // Cold Reset normal operation
        outl(nabm_bar + 0x2C, glob_ctrl);
        
        for(volatile int i = 0; i < 500000; i++); // Wait a bit
        
        // Reset Mixer
        WriteCodec(0x00, 1);
        
        for(volatile int i = 0; i < 500000; i++); // Wait a bit
        
        // Czekanie na gotowość kodeka (Bit 8 w Global Status - 0x30)
        uint32_t status = inl(nabm_bar + 0x30);
        if (!(status & 0x100)) {
            SerialPort::WriteString("AC97: Codec not ready!\n");
        }
        
        // Master volume, Aux, Mono, and PCM out to max (0x0000 means 0 dB attenuation)
        WriteCodec(0x02, 0x0000); // Master volume
        WriteCodec(0x04, 0x0000); // Aux Out
        WriteCodec(0x06, 0x0000); // Mono
        WriteCodec(0x18, 0x0000); // PCM out volume
        WriteCodec(0x26, 0x0000); // Powerdown Ctrl (all on)
        
        // Allocate BDL (32 descriptors = 32 * 8 = 256 bytes)
        void* bdl_phys = PMM::AllocatePage();
        if (bdl_phys) {
            uint64_t bdl_vaddr = (uint64_t)bdl_phys + hhdm_request.response->offset;
            bdl = (AC97BufferDescriptor*)bdl_vaddr;
            
            // Clear BDL
            for (int i = 0; i < 32; i++) {
                bdl[i].buffer_addr = 0;
                bdl[i].length = 0;
                bdl[i].flags = 0;
            }
            
            // Tell NABM where the BDL is
            outl(nabm_bar + 0x10, (uint32_t)(uint64_t)bdl_phys);
        }
    } else {
        SerialPort::WriteString("AC97: Not found.\n");
    }
}

void AC97::PlayWAV(uint8_t* wav_data) {
    if (!nam_bar || !nabm_bar || !bdl) return;
    
    // Stop playback and Reset Registers (CIV, LVI, etc.)
    outb(nabm_bar + 0x1B, 0); // Stop
    outb(nabm_bar + 0x1B, 2); // Reset Registers
    
    // Czekaj aż reset się zakończy
    while (inb(nabm_bar + 0x1B) & 2) {
        for(volatile int i = 0; i < 100; i++);
    }
    
    uint8_t* data = WAV::GetData(wav_data);
    uint32_t size = WAV::GetDataSize(wav_data);
    uint16_t channels = WAV::GetChannels(wav_data);
    uint16_t bits = WAV::GetBitsPerSample(wav_data);
    
    if (!data || size == 0) {
        SerialPort::WriteString("AC97: Invalid WAV data.\n");
        return;
    }
    
    uint32_t sample_rate = WAV::GetSampleRate(wav_data);
    
    if (bits != 16) {
        SerialPort::WriteString("AC97: Only 16-bit PCM supported.\n");
        return;
    }
    
    // Enable Variable Rate Audio (VRA)
    uint16_t ext_stat = ReadCodec(0x2A);
    ext_stat |= 1; // Enable VRA
    WriteCodec(0x2A, ext_stat);
    
    // Set Sample Rate (Front DAC)
    WriteCodec(0x2C, (uint16_t)sample_rate);
    
    uint64_t data_vaddr = (uint64_t)data;
    uint32_t data_phys = (uint32_t)(data_vaddr - hhdm_request.response->offset);
    
    // Length in descriptor is number of samples (frames)
    // 1 frame = channels * (bits / 8) bytes
    uint32_t frame_size = channels * (bits / 8);
    uint32_t samples = size / frame_size;
    
    // Debug info
    SerialPort::WriteString("AC97: Playing WAV. Size: ");
    char buf[16] = {0};
    uint32_t temp = size;
    for (int i = 7; i >= 0; i--) { buf[i] = "0123456789ABCDEF"[temp % 16]; temp /= 16; }
    SerialPort::WriteString(buf);
    SerialPort::WriteString(" SR: ");
    temp = sample_rate;
    for (int i = 7; i >= 0; i--) { buf[i] = "0123456789ABCDEF"[temp % 16]; temp /= 16; }
    SerialPort::WriteString(buf);
    SerialPort::WriteString(" CH: ");
    buf[0] = '0' + channels; buf[1] = 0;
    SerialPort::WriteString(buf);
    SerialPort::WriteString("\n");
    
    int num_descriptors = 0;
    while (samples > 0 && num_descriptors < 32) {
        uint32_t chunk_samples = samples > 65534 ? 65534 : samples;
        bdl[num_descriptors].buffer_addr = data_phys;
        bdl[num_descriptors].length = chunk_samples;
        bdl[num_descriptors].flags = 0; 
        
        data_phys += chunk_samples * frame_size;
        samples -= chunk_samples;
        num_descriptors++;
    }
    
    if (num_descriptors > 0) {
        bdl[num_descriptors - 1].flags = 0x8000; // IOC (Interrupt on Completion)
    }
    
    // Restore BDBAR after reset!
    uint32_t bdl_phys_addr = (uint32_t)((uint64_t)bdl - hhdm_request.response->offset);
    outl(nabm_bar + 0x10, bdl_phys_addr);
    
    // Set LVI (Last Valid Index)
    outb(nabm_bar + 0x15, num_descriptors - 1);
    
    // Start playback (Bit 0 = 1, PCM Out Run/Pause)
    outb(nabm_bar + 0x1B, 1);
    
    SerialPort::WriteString("AC97: Playback started!\n");
}

void AC97::PlaySquareWave() {
    if (!nam_bar || !nabm_bar || !bdl) return;
    
    outb(nabm_bar + 0x1B, 0); 
    outb(nabm_bar + 0x1B, 2); 
    
    while (inb(nabm_bar + 0x1B) & 2) {
        for(volatile int i = 0; i < 100; i++);
    }
    
    uint16_t ext_stat = ReadCodec(0x2A);
    WriteCodec(0x2A, ext_stat | 1);
    WriteCodec(0x2C, 44100);
    
    void* buffer_phys = PMM::AllocatePages(8); // 32KB
    if (!buffer_phys) return;
    int16_t* buffer = (int16_t*)((uint64_t)buffer_phys + hhdm_request.response->offset);
    
    // 440 Hz square wave, 44100 SR, 2 channels
    // period = 44100 / 440 = 100 frames. half period = 50 frames
    for (int i = 0; i < 8192; i++) {
        int16_t val = ((i / 50) % 2 == 0) ? 16000 : -16000;
        buffer[i*2] = val;     // Left
        buffer[i*2+1] = val;   // Right
    }
    
    bdl[0].buffer_addr = (uint32_t)(uint64_t)buffer_phys;
    bdl[0].length = 8192; // 8192 frames
    bdl[0].flags = 0x8000;
    
    uint32_t bdl_phys_addr = (uint32_t)((uint64_t)bdl - hhdm_request.response->offset);
    outl(nabm_bar + 0x10, bdl_phys_addr);
    
    outb(nabm_bar + 0x15, 0); 
    outb(nabm_bar + 0x1B, 1); 
    
    SerialPort::WriteString("AC97: Playing Square Wave (Test Tone)!\n");
}
