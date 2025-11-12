#include "fat.h"
#include "sd.h"
#include "rprintf.h"
#include <stddef.h>

// IMPORTANT: The FAT filesystem starts at sector 2048, not sector 0
// Sector 0 contains the MBR with partition table
#define PARTITION_START 2048

// Global variables
char bootSector[512];
char fat_table[8*512];  // 8 sectors for FAT table
struct boot_sector *bs;
unsigned int root_sector;
unsigned int data_region_start;

// Storage for open file metadata
struct file open_files[10];
int num_open_files = 0;

// External putc function from kernel
extern int putc(int data);

// Function to copy memory
void *memcpy(void *dest, const void *src, int n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int fatInit(void) {
    // Read boot sector from the partition start (sector 2048)
    sd_readblock(PARTITION_START, bootSector, 1);
    
    // Point boot_sector struct to the boot sector
    bs = (struct boot_sector *)bootSector;
    
    // Debug: print fs_type bytes
    esp_printf(putc, "fs_type bytes: ");
    for (int i = 0; i < 8; i++) {
        esp_printf(putc, "%02x ", (unsigned char)bs->fs_type[i]);
    }
    esp_printf(putc, "\r\n");

    esp_printf(putc, "fs_type chars: ");
    for (int i = 0; i < 8; i++) {
        char c = bs->fs_type[i];
        if (c >= 32 && c <= 126) {
            esp_printf(putc, "%c", c);
        } else {
            esp_printf(putc, "?");
        }
    }
    esp_printf(putc, "\r\n");
    
    // Print boot sector info
    esp_printf(putc, "Boot signature: 0x%x\r\n", bs->boot_signature);
    esp_printf(putc, "Bytes per sector: %d\r\n", bs->bytes_per_sector);
    esp_printf(putc, "Sectors per cluster: %d\r\n", bs->num_sectors_per_cluster);
    esp_printf(putc, "Reserved sectors: %d\r\n", bs->num_reserved_sectors);
    esp_printf(putc, "Number of FATs: %d\r\n", bs->num_fat_tables);
    esp_printf(putc, "Sectors per FAT: %d\r\n", bs->num_sectors_per_fat);
    
    // Validate boot signature (should be 0xAA55)
    if (bs->boot_signature != 0xAA55) {
        esp_printf(putc, "ERROR: Invalid boot signature\r\n");
        return -1;
    }
    
    // Validate filesystem type (compare first 5 characters only)
    int is_fat12 = 1;
    int is_fat16 = 1;
    
    for (int i = 0; i < 5; i++) {
        if (bs->fs_type[i] != "FAT12"[i]) is_fat12 = 0;
        if (bs->fs_type[i] != "FAT16"[i]) is_fat16 = 0;
    }
    
    if (!is_fat12 && !is_fat16) {
        esp_printf(putc, "ERROR: Not a FAT12/FAT16 filesystem\r\n");
        return -1;
    }
    
    if (is_fat16) {
        esp_printf(putc, "Filesystem type: FAT16\r\n");
    } else {
        esp_printf(putc, "Filesystem type: FAT12\r\n");
    }
    
    // Read FAT table from disk
    int fat_start = PARTITION_START + bs->num_reserved_sectors;
    int sectors_to_read = (bs->num_sectors_per_fat < 8) ? bs->num_sectors_per_fat : 8;
    for (int i = 0; i < sectors_to_read; i++) {
        sd_readblock(fat_start + i, fat_table + (i * 512), 1);
    }
    
    // Compute root directory sector location
    root_sector = PARTITION_START + bs->num_fat_tables * bs->num_sectors_per_fat + bs->num_reserved_sectors;
    
    // Compute data region start
    int root_dir_sectors = (bs->num_root_dir_entries * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;
    data_region_start = root_sector + root_dir_sectors;
    
    esp_printf(putc, "Root directory at sector: %d\r\n", root_sector);
    esp_printf(putc, "Data region starts at sector: %d\r\n", data_region_start);
    
    return 0;  // Success
}

int fatOpen(const char *filename) {
    char root_dir_buffer[512];
    struct root_directory_entry *entries;
    
    // Parse filename into name and extension
    char name[8];
    char ext[3];
    
    // Initialize with spaces
    for (int i = 0; i < 8; i++) name[i] = ' ';
    for (int i = 0; i < 3; i++) ext[i] = ' ';
    
    // Split filename at the dot
    int i = 0, name_idx = 0;
    while (filename[i] != '\0' && filename[i] != '.' && name_idx < 8) {
        if (filename[i] >= 'a' && filename[i] <= 'z') {
            name[name_idx++] = filename[i] - 32;  // Convert to uppercase
        } else {
            name[name_idx++] = filename[i];
        }
        i++;
    }
    
    if (filename[i] == '.') {
        i++;  // Skip the dot
        int ext_idx = 0;
        while (filename[i] != '\0' && ext_idx < 3) {
            if (filename[i] >= 'a' && filename[i] <= 'z') {
                ext[ext_idx++] = filename[i] - 32;  // Convert to uppercase
            } else {
                ext[ext_idx++] = filename[i];
            }
            i++;
        }
    }
    
    esp_printf(putc, "Looking for: '");
    for (int k = 0; k < 8; k++) esp_printf(putc, "%c", name[k]);
    esp_printf(putc, "' . '");
    for (int k = 0; k < 3; k++) esp_printf(putc, "%c", ext[k]);
    esp_printf(putc, "'\r\n");
    
    // Calculate number of sectors in root directory
    int root_dir_sectors = (bs->num_root_dir_entries * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector;
    
    esp_printf(putc, "Searching %d sectors starting at sector %d\r\n", root_dir_sectors, root_sector);
    
    // Search through root directory entries
    for (int sector = 0; sector < root_dir_sectors; sector++) {
        sd_readblock(root_sector + sector, root_dir_buffer, 1);
        
        if (sector == 0) {
    esp_printf(putc, "\r\nFirst 128 bytes of root directory:\r\n");
    for (int k = 0; k < 128; k++) {
        esp_printf(putc, "%02x ", (unsigned char)root_dir_buffer[k]);
        if ((k + 1) % 16 == 0) esp_printf(putc, "\r\n");
    }
    esp_printf(putc, "\r\n");
}

        entries = (struct root_directory_entry *)root_dir_buffer;
        
        int entries_per_sector = bs->bytes_per_sector / 32;
        
        for (int j = 0; j < entries_per_sector; j++) {
            // Check if entry is empty (first byte is 0x00)
            if (entries[j].file_name[0] == 0x00) {
				esp_printf(putc, "  Entry %d: END OF DIRECTORY\r\n", j);
				return -1;
			}
            
            // Check if entry is deleted (first byte is 0xE5)
            if ((unsigned char)entries[j].file_name[0] == 0xE5) {
                esp_printf(putc, "  Entry %d: DELETED\r\n", j);
                continue;
            }
            
            // Print the entry details
            esp_printf(putc, "  Entry %d: '", j);
            for (int k = 0; k < 8; k++) esp_printf(putc, "%c", entries[j].file_name[k]);
            esp_printf(putc, "' . '");
            for (int k = 0; k < 3; k++) esp_printf(putc, "%c", entries[j].file_extension[k]);
            esp_printf(putc, "' attr=0x%02x cluster=%d size=%d\r\n", 
                      entries[j].attribute, entries[j].cluster, entries[j].file_size);
            
            // Skip long filename entries FIRST
            if ((entries[j].attribute & 0x0F) == 0x0F) {
                esp_printf(putc, "    -> Skipping (long filename entry)\r\n");
                continue;
            }
            
            // Skip volume labels
            if (entries[j].attribute & 0x08) {
                esp_printf(putc, "    -> Skipping (volume label)\r\n");
                continue;
            }
            
            // Compare name
            esp_printf(putc, "    -> Comparing... ");
            int name_match = 1;
            for (int k = 0; k < 8; k++) {
                if (name[k] != entries[j].file_name[k]) {
                    name_match = 0;
                    break;
                }
            }
            
            int ext_match = 1;
            for (int k = 0; k < 3; k++) {
                if (ext[k] != entries[j].file_extension[k]) {
                    ext_match = 0;
                    break;
                }
            }
            
            if (name_match && ext_match) {
                esp_printf(putc, "MATCH!\r\n");
                esp_printf(putc, "  Found file! Cluster: %d, Size: %d bytes\r\n", 
                          entries[j].cluster, entries[j].file_size);
                
                if (num_open_files >= 10) {
                    esp_printf(putc, "ERROR: Too many open files\r\n");
                    return -1;
                }
                
                memcpy(&open_files[num_open_files].rde, &entries[j], 32);
                open_files[num_open_files].start_cluster = entries[j].cluster;
                
                return num_open_files++;
            } else {
                esp_printf(putc, "no match (name=%d ext=%d)\r\n", name_match, ext_match);
            }
        }
    }
    
    esp_printf(putc, "ERROR: File not found after searching all entries\r\n");
    return -1;
}

// Helper function to get next cluster from FAT
uint16_t get_next_cluster(uint16_t current_cluster) {
    uint16_t next_cluster;
    
    // Check if FAT16 (compare first 5 chars)
    int is_fat16 = 1;
    for (int i = 0; i < 5; i++) {
        if (bs->fs_type[i] != "FAT16"[i]) {
            is_fat16 = 0;
            break;
        }
    }
    
    if (is_fat16) {
        // FAT16: each entry is 2 bytes
        uint16_t *fat16 = (uint16_t *)fat_table;
        next_cluster = fat16[current_cluster];
        
        // Check for end of chain
        if (next_cluster >= 0xFFF8) {
            return 0xFFFF;  // End of chain
        }
    } else {
        // FAT12: each entry is 12 bits
        int fat_offset = current_cluster + (current_cluster / 2);
        unsigned char *fat_ptr = (unsigned char *)&fat_table[fat_offset];
        
        if (current_cluster & 1) {
            // Odd cluster - upper 12 bits
            next_cluster = (fat_ptr[0] >> 4) | (fat_ptr[1] << 4);
        } else {
            // Even cluster - lower 12 bits
            next_cluster = fat_ptr[0] | ((fat_ptr[1] & 0x0F) << 8);
        }
        
        // Check for end of chain
        if (next_cluster >= 0xFF8) {
            return 0xFFFF;  // End of chain
        }
    }
    
    return next_cluster;
}

int fatRead(int fd, void *buffer, int num_bytes) {
    if (fd < 0 || fd >= num_open_files) {
        esp_printf(putc, "ERROR: Invalid file descriptor\r\n");
        return -1;
    }
    
    struct file *f = &open_files[fd];
    uint32_t file_size = f->rde.file_size;
    
    // Don't read more than file size
    if (num_bytes > file_size) {
        num_bytes = file_size;
    }
    
    esp_printf(putc, "Reading %d bytes from file (size: %d)\r\n", num_bytes, file_size);
    
    char *buf = (char *)buffer;
    int bytes_read = 0;
    uint16_t current_cluster = f->start_cluster;
    char cluster_buffer[512 * 4];  // Buffer for full cluster
    
    while (bytes_read < num_bytes && current_cluster != 0xFFFF && current_cluster >= 2) {
        // Convert cluster number to sector number
        uint32_t sector = data_region_start + (current_cluster - 2) * bs->num_sectors_per_cluster;
        
        esp_printf(putc, "Reading cluster %d at sector %d\r\n", current_cluster, sector);
        
        // Read all sectors in this cluster
        for (int i = 0; i < bs->num_sectors_per_cluster; i++) {
            sd_readblock(sector + i, cluster_buffer + (i * 512), 1);
        }
        
        // Calculate bytes to copy from this cluster
        int cluster_size = bs->num_sectors_per_cluster * 512;
        int bytes_to_copy = cluster_size;
        if (bytes_read + bytes_to_copy > num_bytes) {
            bytes_to_copy = num_bytes - bytes_read;
        }
        
        memcpy(buf + bytes_read, cluster_buffer, bytes_to_copy);
        bytes_read += bytes_to_copy;
        
        // Get next cluster if needed
        if (bytes_read < num_bytes) {
            current_cluster = get_next_cluster(current_cluster);
        }
    }
    
    esp_printf(putc, "Read %d bytes total\r\n", bytes_read);
    return bytes_read;
}
