#ifndef __SD_H__
#define __SD_H__
#include <stdint.h>

#include <stdint.h>

// I/O port access functions
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);

#define SECTOR_SIZE 512

// ATA I/O ports
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

// ATA Commands
#define ATA_CMD_READ_SECTORS  0x20

// ATA Status bits
#define ATA_STATUS_BSY  0x80  // Busy
#define ATA_STATUS_DRDY 0x40  // Drive ready
#define ATA_STATUS_DRQ  0x08  // Data request ready
#define ATA_STATUS_ERR  0x01  // Error

// Function declarations
void sd_init(void);
void sd_readblock(uint32_t sector_num, char *buf, uint32_t num_sectors);

// Helper functions
void ata_wait_busy(void);
void ata_wait_drq(void);

#endif
