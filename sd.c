#include "sd.h"
#include <stdint.h>

// Need inb/outb for I/O port access
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t val);
extern void insl(uint16_t port, void *addr, uint32_t cnt);

// Wait for disk to not be busy
void ata_wait_busy(void) {
    while (inb(ATA_STATUS) & ATA_STATUS_BSY) {
        // Wait
    }
}

// Wait for disk to be ready for data transfer
void ata_wait_drq(void) {
    while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ)) {
        // Wait
    }
}

void sd_init(void) {
    // Wait for drive to be ready
    ata_wait_busy();
}

void sd_readblock(uint32_t sector_num, char *buf, uint32_t num_sectors) {
    // Wait for drive to be ready
    ata_wait_busy();
    
    // Set up for LBA28 mode
    outb(ATA_DRIVE, 0xE0 | ((sector_num >> 24) & 0x0F));  // Master drive, LBA mode
    outb(ATA_SECTOR_CNT, num_sectors);                     // Number of sectors
    outb(ATA_LBA_LOW, sector_num & 0xFF);                  // LBA bits 0-7
    outb(ATA_LBA_MID, (sector_num >> 8) & 0xFF);          // LBA bits 8-15
    outb(ATA_LBA_HIGH, (sector_num >> 16) & 0xFF);        // LBA bits 16-23
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);               // Read command
    
    // Read each sector
    for (uint32_t i = 0; i < num_sectors; i++) {
        // Wait for data to be ready
        ata_wait_drq();
        
        // Read 256 words from data port
        uint16_t *dst = (uint16_t *)(buf + i * SECTOR_SIZE);
        for (int j = 0; j < 256; j++) {
            dst[j] = inw(ATA_DATA);
        }
    }
}
