// Multiboot header using inline assembly
#define MULTIBOOT2_HEADER_MAGIC         0xe85250d6
const unsigned int multiboot_header[] __attribute__((section(".multiboot"))) = {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

#include "../rprintf.h"
#include "../interrupt.h"
#include "../page.h"
#include "../sd.h"
#include "../fat.h"

// External symbols from linker script
extern int _end_kernel;

// External page directory from page.c
extern struct page_directory_entry pd[1024];

int putc(int data) {
    // Video memory starts at 0xB8000
    volatile unsigned short* vram = (unsigned short*)0xB8000;
    static int cursor_pos = 0;
    
    if (data == '\n') {
        // Move to next line (80 characters per line)
        cursor_pos = ((cursor_pos / 80) + 1) * 80;
    } else if (data == '\r') {
        // Carriage return - move to beginning of current line
        cursor_pos = (cursor_pos / 80) * 80;
    } else {
        // Write character with gray color (0x07) on black background
        vram[cursor_pos] = (0x07 << 8) | (unsigned char)data;
        cursor_pos++;
    }
    
    // Scroll if reached bottom of screen (25 rows total, 0-24)
    if (cursor_pos >= 80 * 25) {
        // Scroll entire screen up by one line
        for (int i = 0; i < 80 * 24; i++) {
            vram[i] = vram[i + 80];
        }
        // Clear the bottom line
        for (int i = 80 * 24; i < 80 * 25; i++) {
            vram[i] = (0x07 << 8) | ' ';
        }
        // Move cursor to start of last line
        cursor_pos = 80 * 24;
    }
    
    return data;
}

// Wrapper function for esp_printf compatibility
int putc_wrapper(int data) {
    putc(data);
    return data;
}

void main() {
    // Clear the screen first
    volatile unsigned short* vram = (unsigned short*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) {
        vram[i] = (0x07 << 8) | ' ';
    }
    
    // Initialize interrupt system for keyboard input
    remap_pic();  // Set up the programmable interrupt controller
    load_gdt();   // Load the global descriptor table
    init_idt();   // Initialize the interrupt descriptor table
    asm("sti");   // Enable interrupts
    
    // Print welcome message
    esp_printf(putc_wrapper, "CS310 Homework 5: Fat Fs Driver\r\n");
    esp_printf(putc_wrapper, "Interrupts enabled. Type to test keyboard input:\r\n");
    esp_printf(putc_wrapper, "\r\n");
    

	init_pfa_list();
	esp_printf(putc_wrapper, "Page frame allocator initialized\r\n");
    
    // Identity map the kernel
// Map from 0x100000 (1MB) to end of kernel
for (uint32_t addr = 0x100000; addr < (uint32_t)&_end_kernel; addr += 0x1000) {
    struct ppage tmp;
    tmp.next = NULL;
    tmp.prev = NULL;
    tmp.physical_addr = (void *)addr;
    map_pages((void *)addr, &tmp, pd);
}

// Identity map the video buffer at 0xB8000
	struct ppage video_page;
	video_page.next = NULL;
	video_page.prev = NULL;
	video_page.physical_addr = (void *)0xB8000;
	map_pages((void *)0xB8000, &video_page, pd);

// Identity map the stack
	uint32_t esp;
	asm("mov %%esp,%0" : "=r" (esp));
	uint32_t stack_start = esp & 0xFFFFF000;  // Round down to page boundary
	for (uint32_t addr = stack_start; addr < stack_start + 0x10000; addr += 0x1000) {
		struct ppage tmp;
		tmp.next = NULL;
		tmp.prev = NULL;
		tmp.physical_addr = (void *)addr;
		map_pages((void *)addr, &tmp, pd);
}

esp_printf(putc_wrapper, "Identity mapping complete\r\n");

// Load page directory and enable paging
asm("mov %0,%%cr3" : : "r"(pd));
asm("mov %%cr0, %%eax\n"
    "or $0x80000001,%%eax\n"
    "mov %%eax,%%cr0" : : : "eax");

esp_printf(putc_wrapper, "Paging enabled!\r\n");
    
    struct ppage *pages = allocate_physical_pages(10);
if (pages != NULL) {
    esp_printf(putc_wrapper, "Allocated 10 pages successfully\r\n");
    free_physical_pages_list(pages);
    esp_printf(putc_wrapper, "Freed 10 pages\r\n");
}
esp_printf(putc_wrapper, "\r\n");
    
    
esp_printf(putc_wrapper, "\r\n=== Testing FAT Filesystem ===\r\n");

// Initialize SD card driver
esp_printf(putc_wrapper, "Initializing SD card...\r\n");
sd_init();

// Initialize FAT filesystem
esp_printf(putc_wrapper, "Initializing FAT filesystem...\r\n");
if (fatInit() == 0) {
    esp_printf(putc_wrapper, "FAT filesystem initialized successfully!\r\n");
    
    // Open test file
    esp_printf(putc_wrapper, "\r\nOpening test.txt...\r\n");
    int fd = fatOpen("test.txt");  // Changed from TEST.TXT to test.txt
    
    if (fd >= 0) {
        esp_printf(putc_wrapper, "File opened successfully! fd=%d\r\n", fd);
        
        // Read file contents
        char buffer[256];
        int bytes_read = fatRead(fd, buffer, 255);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // Null terminate
            esp_printf(putc_wrapper, "\r\n=== File contents ===\r\n");
            esp_printf(putc_wrapper, "%s", buffer);
            esp_printf(putc_wrapper, "\r\n=== End of file (%d bytes) ===\r\n", bytes_read);
        } else {
            esp_printf(putc_wrapper, "ERROR: Failed to read file\r\n");
        }
    } else {
        esp_printf(putc_wrapper, "ERROR: Failed to open file\r\n");
    }
} else {
    esp_printf(putc_wrapper, "ERROR: Failed to initialize FAT filesystem\r\n");
}

esp_printf(putc_wrapper, "\r\n=== FAT Test Complete ===\r\n\r\n");

    // Infinite loop - wait for keyboard interrupts
    while(1) {
        asm("hlt");  // Halt until next interrupt
    }
}
