#include "ata.h"
#include "../cpu/io.h"
#include "../serial.h"
#include "../cpu/critical.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_COMM_STAT    0x1F7
#define ATA_PRIMARY_ALTSTAT_CTRL 0x3F6

void ATA::Wait() {
    for (int i = 0; i < 4; i++) {
        inb(ATA_PRIMARY_ALTSTAT_CTRL);
    }
}

void ATA::Init() {
    // Select master drive
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
    Wait();
    
    // Check if connected
    uint8_t status = inb(ATA_PRIMARY_COMM_STAT);
    if (status == 0xFF) {
        SerialPort::WriteString("ATA: Primary Master Drive not found.\n");
        return;
    }
    SerialPort::WriteString("ATA: Primary Master Drive initialized.\n");
}

bool ATA::ReadSector(uint32_t lba, uint8_t* buffer) {
    return ReadSectors(lba, 1, buffer);
}

bool ATA::ReadSectors(uint32_t lba, uint8_t count, uint8_t* buffer) {
    EnterCritical();
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_ERR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Command 0x20: Read Sectors with Retry
    outb(ATA_PRIMARY_COMM_STAT, 0x20);
    
    uint16_t* ptr = (uint16_t*)buffer;
    
    for (int i = 0; i < count; i++) {
        uint8_t status = inb(ATA_PRIMARY_COMM_STAT);
        while ((status & 0x80) && !(status & 0x01)) { // BSY set and ERR clear
            status = inb(ATA_PRIMARY_COMM_STAT);
        }
        
        if (status & 0x01) { // ERR set
            SerialPort::WriteString("ATA: Read Error!\n");
            ExitCritical();
            return false;
        }

        while (!(status & 0x08)) { // DRQ clear
            status = inb(ATA_PRIMARY_COMM_STAT);
        }

        // Read 256 words (512 bytes)
        insw(ATA_PRIMARY_DATA, ptr, 256);
        ptr += 256;
    }

    ExitCritical();
    return true;
}

bool ATA::WriteSector(uint32_t lba, const uint8_t* buffer) {
    return WriteSectors(lba, 1, buffer);
}

bool ATA::WriteSectors(uint32_t lba, uint8_t count, const uint8_t* buffer) {
    EnterCritical();
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_ERR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Command 0x30: Write Sectors with Retry
    outb(ATA_PRIMARY_COMM_STAT, 0x30);
    
    const uint16_t* ptr = (const uint16_t*)buffer;
    
    for (int i = 0; i < count; i++) {
        uint8_t status = inb(ATA_PRIMARY_COMM_STAT);
        while ((status & 0x80) && !(status & 0x01)) { // BSY set and ERR clear
            status = inb(ATA_PRIMARY_COMM_STAT);
        }
        
        if (status & 0x01) { // ERR set
            SerialPort::WriteString("ATA: Write Error!\n");
            ExitCritical();
            return false;
        }
        
        while (!(status & 0x08)) { // DRQ clear
            status = inb(ATA_PRIMARY_COMM_STAT);
        }
        
        // Write 256 words (512 bytes)
        outsw(ATA_PRIMARY_DATA, ptr, 256);
        ptr += 256;
    }
    
    // Flush cache
    outb(ATA_PRIMARY_COMM_STAT, 0xE7);
    uint8_t status = inb(ATA_PRIMARY_COMM_STAT);
    while (status & 0x80) { // BSY set
        status = inb(ATA_PRIMARY_COMM_STAT);
    }

    ExitCritical();
    return true;
}
