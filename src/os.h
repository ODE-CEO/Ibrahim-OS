#ifndef OS_H
#define OS_H

typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed int     int32_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned int   uintptr_t;

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED   0xFF0000

#define KEY_LEFT    0x100
#define KEY_RIGHT   0x101
#define KEY_UP      0x102
#define KEY_DOWN    0x103

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint32_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

extern uint32_t total_ram_mb;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void vga_init(multiboot_info_t* mbi);
void put_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_char(char c, int x, int y, uint32_t color);
void draw_string(const char* str, int x, int y, uint32_t color);
void draw_multiline(const char* str, int x, int y, uint32_t color);
void draw_mouse(int x, int y);
void swap_buffers(void);

void ps2_init(void);
void ps2_poll(void);

void gui_process_clicks(void);
void gui_draw(void);

extern int last_key_pressed;

int str_cmp(const char* a, const char* b);
void str_copy(char* dest, const char* src);
int str_len(const char* s);
void itoa(int n, char* buf);

#endif
