#include "os.h"

int mouse_x = 320, mouse_y = 240;
int left_clicked = 0, right_clicked = 0;
int last_key_pressed = 0;

static uint8_t mouse_bytes[3];
static int mouse_cycle = 0;
static int extended_key = 0;
static int shift_down = 0;

const char kbd_US[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0, '*',
    0,  ' '
};

static char turkish_key(uint8_t scan_code) {
    if (scan_code == 0x1A) return 'g';
    if (scan_code == 0x1B) return 'u';
    if (scan_code == 0x27) return 's';
    if (scan_code == 0x28) return 'i';
    if (scan_code == 0x33) return 'o';
    if (scan_code == 0x34) return 'c';
    return kbd_US[scan_code];
}

static inline void ps2_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 2));
}

static inline void ps2_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 1));
}

void ps2_init(void) {
    ps2_wait_input();
    outb(0x64, 0xA8);

    ps2_wait_input();
    outb(0x64, 0x20);
    ps2_wait_output();
    uint8_t status = inb(0x60) | 2;
    status &= ~0x20;

    ps2_wait_input();
    outb(0x64, 0x60);
    ps2_wait_input();
    outb(0x60, status);

    ps2_wait_input();
    outb(0x64, 0xD4);
    ps2_wait_input();
    outb(0x60, 0xF6);
    ps2_wait_output();
    inb(0x60);

    ps2_wait_input();
    outb(0x64, 0xD4);
    ps2_wait_input();
    outb(0x60, 0xF4);
    ps2_wait_output();
    inb(0x60);

    while (inb(0x64) & 1) {
        inb(0x60);
    }
}

void ps2_poll(void) {
    while (1) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) break;

        uint8_t data = inb(0x60);

        if (status & 0x20) {
            if (mouse_cycle == 0) {
                if ((data & 0x08) == 0) {
                    continue;
                }
            }

            mouse_bytes[mouse_cycle++] = data;

            if (mouse_cycle == 3) {
                mouse_cycle = 0;

                if (mouse_bytes[0] & 0xC0) {
                    continue;
                }

                int dx = (int8_t)mouse_bytes[1];
                int dy = (int8_t)mouse_bytes[2];

                if (mouse_bytes[0] & 0x10) dx |= 0xFFFFFF00;
                if (mouse_bytes[0] & 0x20) dy |= 0xFFFFFF00;

                mouse_x += dx;
                mouse_y -= dy;

                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x > SCREEN_WIDTH - 6) mouse_x = SCREEN_WIDTH - 6;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y > SCREEN_HEIGHT - 6) mouse_y = SCREEN_HEIGHT - 6;

                left_clicked  = mouse_bytes[0] & 1;
                right_clicked = (mouse_bytes[0] >> 1) & 1;
            }
        } else {
            if (data == 0xE0) {
                extended_key = 1;
            } else if (data == 0x2A || data == 0x36) {
                shift_down = 1;
            } else if (data == 0xAA || data == 0xB6) {
                shift_down = 0;
            } else if (extended_key) {
                extended_key = 0;
                if (data < 128) {
                    if (data == 0x4B) last_key_pressed = KEY_LEFT;
                    else if (data == 0x4D) last_key_pressed = KEY_RIGHT;
                    else if (data == 0x48) last_key_pressed = KEY_UP;
                    else if (data == 0x50) last_key_pressed = KEY_DOWN;
                }
            } else if (data < 128) {
                char key = turkish_key(data);
                if (shift_down && key >= 'a' && key <= 'z') key -= 'a' - 'A';
                last_key_pressed = key;
            }
        }
    }
}
