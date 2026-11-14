#ifndef CU_RUNNER_H
#define CU_RUNNER_H

#include "os.h"

#define MAX_CU_BUTTONS 8

typedef struct {
    int active;
    int x, y, w, h;
    char text[20];
    char action[10];
    char arg[30];
} cu_button_t;

typedef struct {
    char title[32];
    uint32_t bg_color;
    cu_button_t buttons[MAX_CU_BUTTONS];
    int button_count;
    char raw_code[1024];
} cu_app_context_t;

extern cu_app_context_t current_cu_app;

void cu_init(void);
void cu_run_script(const char* script);
void cu_draw_app(int wx, int wy, int ww, int wh);
void cu_handle_click(int wx, int wy, int mx, int my);

#endif
