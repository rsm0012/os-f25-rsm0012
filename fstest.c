
#include "fat.h"
#include "sd.h"
#include "rprintf.h"

extern int putc(int data);

void test_fat_filesystem() {
    esp_printf(putc, "\r\n=== FAT Filesystem Test ===\r\n\r\n");
    
    // Initialize SD driver
    esp_printf(putc, "Initializing SD driver...\r\n");
    sd_init();
    
    // Initialize FAT filesystem
    esp_printf(putc, "Initializing FAT filesystem...\r\n");
    if (fatInit() != 0) {
        esp_printf(putc, "ERROR: FAT init failed\r\n");
        return;
    }
    
    // Open test.txt
    esp_printf(putc, "\r\nOpening test.txt...\r\n");
    int fd = fatOpen("test.txt");
    if (fd < 0) {
        esp_printf(putc, "ERROR: Could not open test.txt\r\n");
        return;
    }
    esp_printf(putc, "File opened successfully (fd=%d)\r\n", fd);
    
    // Read file contents
    esp_printf(putc, "\r\nReading file...\r\n");
    char buffer[512];
    int bytes = fatRead(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        esp_printf(putc, "\r\n--- File Contents (%d bytes) ---\r\n", bytes);
        esp_printf(putc, "%s", buffer);
        esp_printf(putc, "\r\n--- End of File ---\r\n");
    } else {
        esp_printf(putc, "ERROR: Read failed\r\n");
    }
    
    esp_printf(putc, "\r\n=== Test Complete ===\r\n");
}
