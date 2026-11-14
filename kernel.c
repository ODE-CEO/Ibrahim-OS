#include "os.h"
#include "fs.h"
#include "speaker.h"

uint32_t total_ram_mb = 128;
extern void paint_init(void);

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    if (magic != 0x2BADB002 || !mbi) {
        return;
    }

    if (mbi->flags & 1) {
        total_ram_mb = (mbi->mem_upper + 1024) / 1024;
    }

    vga_init(mbi);
    ps2_init();
    fs_init();
    paint_init();
    play_startup_melody();

    while(1) {
        ps2_poll();
        gui_process_clicks();
        gui_draw();
        swap_buffers();
    }
}
