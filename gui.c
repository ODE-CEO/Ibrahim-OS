#include "os.h"
#include "fs.h"
#include "speaker.h"
#include "cu_runner.h"
#include "c_compiler.h"

// --- STRİNG YARDIMCILARI ---
int str_cmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}
void str_copy(char* dest, const char* src) {
    int i = 0; while (src[i]) { dest[i] = src[i]; i++; } dest[i] = '\0';
}
int str_len(const char* s) { int i = 0; while(s[i]) i++; return i; }
void itoa(int n, char* buf) { 
    int i=0, sign=n; 
    if(n<0) n=-n; 
    do { buf[i++]=n%10+'0'; } while((n/=10)>0); 
    if(sign<0) buf[i++]='-'; 
    buf[i]='\0'; 
    for(int j=0;j<i/2;j++){char t=buf[j];buf[j]=buf[i-j-1];buf[i-j-1]=t;} 
}

int str_ends_with(const char* str, const char* suffix) {
    int str_l = str_len(str);
    int suf_l = str_len(suffix);
    if (str_l < suf_l) return 0;
    return (str_cmp(str + str_l - suf_l, suffix) == 0);
}

// --- DONANIM DEĞİŞKENLERİ ---
extern int left_clicked;
extern int right_clicked; 
extern int mouse_x, mouse_y;

// --- CMOS RTC SAAT ---
uint8_t rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

void get_rtc_time_str(char* buf) {
    outb(0x70, 0x0A);
    while (inb(0x71) & 0x80);

    uint8_t sec  = rtc_read(0x00);
    uint8_t min  = rtc_read(0x02);
    uint8_t hour = rtc_read(0x04);
    uint8_t regb = rtc_read(0x0B);

    if (!(regb & 0x04)) {
        sec  = ((sec / 16) * 10) + (sec & 0x0F);
        min  = ((min / 16) * 10) + (min & 0x0F);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    }
    if (!(regb & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    buf[0] = '0' + (hour / 10);
    buf[1] = '0' + (hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (min / 10);
    buf[4] = '0' + (min % 10);
    buf[5] = ':';
    buf[6] = '0' + (sec / 10);
    buf[7] = '0' + (sec % 10);
    buf[8] = '\0';
}

// --- PENCERE YÖNETİCİSİ VE Z-INDEX ---
typedef struct {
    int id; // 0:Wel, 1:Not, 2:Cal, 3:Trm, 4:Exp, 5:Pnt, 6:Set, 7:LB_App
    int open, minimized, maximized;
    int x, y, w, h;
    int px, py, pw, ph; 
    int dragging, drag_ox, drag_oy;
    int resizing; 
} Window;

#define WINDOW_COUNT 8
Window w_wel = {0, 1,0,0, 160,120,320,155, 0,0,0,0, 0,0,0,0};
Window w_not = {1, 0,0,0, 150,50,320,250, 0,0,0,0, 0,0,0,0};
Window w_cal = {2, 0,0,0, 80,50,210,220, 0,0,0,0, 0,0,0,0};
Window w_trm = {3, 0,0,0, 200,100,380,260, 0,0,0,0, 0,0,0,0};
Window w_exp = {4, 0,0,0, 100,60,400,300, 0,0,0,0, 0,0,0,0};
Window w_pnt = {5, 0,0,0, 120,40,360,280, 0,0,0,0, 0,0,0,0};
Window w_set = {6, 0,0,0, 140,50,380,310, 0,0,0,0, 0,0,0,0};
Window w_app = {7, 0,0,0, 160,60,350,280, 0,0,0,0, 0,0,0,0}; // LB Runtime Window

Window* window_stack[WINDOW_COUNT] = {&w_wel, &w_cal, &w_not, &w_trm, &w_exp, &w_pnt, &w_set, &w_app};
int focused_app = 0;

void bring_to_front(Window* w) {
    int found = -1;
    for(int i = 0; i < WINDOW_COUNT; i++) {
        if (window_stack[i] == w) { found = i; break; }
    }
    if (found != -1 && found < WINDOW_COUNT - 1) {
        for(int i = found; i < WINDOW_COUNT - 1; i++) {
            window_stack[i] = window_stack[i+1];
        }
        window_stack[WINDOW_COUNT - 1] = w;
    }
}

static void open_terminal_window(void) {
    w_trm.open = 1;
    w_trm.minimized = 0;
    bring_to_front(&w_trm);
}

void launch_cu_app(const char* code) {
    cu_run_script(code);
    w_app.open = 1;
    w_app.minimized = 0;
    focused_app = 7;
    bring_to_front(&w_app);
    sound_open();
}

void launch_file(int node_id) {
    int kind = fs_get_kind(node_id);
    open_terminal_window();
    if (kind == FS_KIND_EXECUTABLE) {
        c_compile_and_run(fs_table[node_id].content);
        focused_app = 3;
        bring_to_front(&w_trm);
    }
}

// --- MASAÜSTÜ VE AYARLAR ---
uint32_t desktop_bg_color = 0x0078D7;
int desktop_wallpaper = 0;
int start_menu_open = 0;

int ctx_menu_open = 0;
int ctx_menu_x = 0, ctx_menu_y = 0;
int ctx_target_dir = 0;
int ctx_target_node = -1;
int file_panel_open = 0;
int file_panel_mode = 0;
int file_panel_node = -1;
char file_panel_input[FS_NAME_MAX] = "";
int file_panel_input_len = 0;

int opened_file_id = -1;
int notepad_cursor = 0;
int current_exp_dir = 0;
int dragging_file_id = -1;
int drag_f_ox = 0, drag_f_oy = 0;

static int gui_frame_counter = 0;
static int last_clicked_node = -1;
static int last_clicked_frame = -1000;
static int welcome_name_len = 0;
static int welcome_complete = 0;
static char welcome_name[32] = "";

int is_hover(int x, int y, int w, int h);
extern char notepad_temp[FS_FILE_MAX_SIZE];

// --- İBRAHİM PAINT ---
#define PAINT_W 340
#define PAINT_H 200
uint32_t paint_canvas[PAINT_W * PAINT_H];
uint32_t current_paint_color = COLOR_BLACK;
int paint_brush_size = 1;

void paint_init(void) {
    for (int i = 0; i < PAINT_W * PAINT_H; i++) {
        paint_canvas[i] = COLOR_WHITE;
    }
}

// --- TERMİNAL ---
char term_history[8][60]; 
char term_input[40] = "";
int term_input_len = 0;

static int is_double_click(int node_id) {
    int is_double = last_clicked_node == node_id &&
        gui_frame_counter - last_clicked_frame <= 150;
    last_clicked_node = node_id;
    last_clicked_frame = gui_frame_counter;
    return is_double;
}

static void copy_extension_name(char* dest, const char* source, const char* extension) {
    int i = 0;
    while (source[i] && source[i] != '.' && i < FS_NAME_MAX - 1) {
        dest[i] = source[i];
        i++;
    }
    dest[i++] = '.';
    int j = 0;
    while (extension[j] && i < FS_NAME_MAX - 1) dest[i++] = extension[j++];
    dest[i] = '\0';
}

static char* notepad_buffer(void) {
    return opened_file_id == -1 ? notepad_temp : fs_table[opened_file_id].content;
}

static int notepad_length(void) {
    return opened_file_id == -1 ? str_len(notepad_temp) : fs_table[opened_file_id].size;
}

static void notepad_clamp_cursor(void) {
    int length = notepad_length();
    if (notepad_cursor < 0) notepad_cursor = 0;
    if (notepad_cursor > length) notepad_cursor = length;
}

static void notepad_insert(char c) {
    if (opened_file_id == -1) {
        int length = str_len(notepad_temp);
        if (length >= FS_FILE_MAX_SIZE - 1) return;
        for (int i = length; i >= notepad_cursor; i--) notepad_temp[i + 1] = notepad_temp[i];
        notepad_temp[notepad_cursor] = c;
    } else if (!fs_insert_char(opened_file_id, notepad_cursor, c)) {
        return;
    }
    notepad_cursor++;
}

static void notepad_backspace(void) {
    if (notepad_cursor <= 0) return;
    if (opened_file_id == -1) {
        int length = str_len(notepad_temp);
        for (int i = notepad_cursor - 1; i < length; i++) notepad_temp[i] = notepad_temp[i + 1];
    } else {
        int length = fs_table[opened_file_id].size;
        for (int i = notepad_cursor - 1; i < length; i++) {
            fs_table[opened_file_id].content[i] = fs_table[opened_file_id].content[i + 1];
        }
        fs_table[opened_file_id].size = length - 1;
    }
    notepad_cursor--;
}

static void notepad_move_vertical(int direction) {
    char* text = notepad_buffer();
    int cursor = notepad_cursor;
    int line_start = cursor;
    while (line_start > 0 && text[line_start - 1] != '\n') line_start--;
    int column = cursor - line_start;
    int target = direction < 0 ? line_start - 1 : cursor;

    if (direction < 0) {
        if (target < 0) return;
        while (target > 0 && text[target - 1] != '\n') target--;
        int previous_start = target;
        while (target < line_start - 1 && text[target] != '\n') target++;
        int previous_length = target - previous_start;
        notepad_cursor = previous_start + (column < previous_length ? column : previous_length);
    } else {
        while (text[target] && text[target] != '\n') target++;
        if (!text[target]) return;
        target++;
        int next_start = target;
        while (text[target] && text[target] != '\n') target++;
        int next_length = target - next_start;
        notepad_cursor = next_start + (column < next_length ? column : next_length);
    }
}

static void open_notepad_file(int node_id) {
    opened_file_id = node_id;
    w_not.open = 1;
    w_not.minimized = 0;
    focused_app = 1;
    notepad_cursor = opened_file_id == -1 ? str_len(notepad_temp) : fs_table[node_id].size;
    bring_to_front(&w_not);
    sound_open();
}

static void open_file_panel(int node_id, int mode) {
    file_panel_open = 1;
    file_panel_mode = mode;
    file_panel_node = node_id;
    file_panel_input_len = 0;
    file_panel_input[0] = '\0';
    if (mode == 1 && node_id != -1) {
        str_copy(file_panel_input, fs_table[node_id].name);
        file_panel_input_len = str_len(file_panel_input);
    }
    sound_click();
}

static void create_source_file(int parent_id, const char* extension) {
    char name[FS_NAME_MAX] = "yeni_dosya";
    int suffix = 1;
    char candidate[FS_NAME_MAX];
    copy_extension_name(candidate, name, extension);
    while (fs_find_child(parent_id, candidate) != -1 && suffix < 10) {
        name[10] = '0' + suffix++;
        name[11] = '\0';
        copy_extension_name(candidate, name, extension);
    }
    int node = fs_create(parent_id, candidate, FS_TYPE_FILE, 20, 20);
    if (node != -1) {
        if (str_cmp(extension, "c") == 0) {
            const char* source = "int main() {\n    print(\"Yeni C programi\");\n    return 0;\n}\n";
            fs_write(node, source, str_len(source));
        } else {
            const char* source = "int main() {\n    print(\"Yeni EXC programi\");\n    return 0;\n}\n";
            fs_write(node, source, str_len(source));
        }
        sound_open();
    } else {
        sound_error();
    }
}

static int source_to_binary(const char* source, char* output, int max_len, int add_header) {
    int out = 0;
    const char* header = add_header ? "IBEXC0;" : "";
    while (*header && out < max_len - 1) output[out++] = *header++;
    for (int i = 0; source[i] && out < max_len - 1; i++) {
        unsigned char value = (unsigned char)source[i];
        for (int bit = 7; bit >= 0 && out < max_len - 1; bit--) {
            output[out++] = (value & (1 << bit)) ? '1' : '0';
        }
    }
    output[out] = '\0';
    return out;
}

static void convert_file(int node_id, const char* extension, int closed) {
    if (node_id < 0 || !fs_is_editable(node_id)) {
        sound_error();
        return;
    }
    char name[FS_NAME_MAX];
    copy_extension_name(name, fs_table[node_id].name, extension);
    if (!fs_rename(node_id, name)) {
        sound_error();
        return;
    }
    if (str_cmp(extension, "exc") == 0 && closed) {
        char binary[FS_FILE_MAX_SIZE];
        source_to_binary(fs_table[node_id].content, binary, FS_FILE_MAX_SIZE, 1);
        fs_write(node_id, binary, str_len(binary));
    } else if (str_cmp(extension, "o") == 0) {
        char binary[FS_FILE_MAX_SIZE];
        source_to_binary(fs_table[node_id].content, binary, FS_FILE_MAX_SIZE, 0);
        fs_write(node_id, binary, str_len(binary));
    }
    sound_open();
}

void term_print(const char* text) {
    for (int i = 0; i < 7; i++) str_copy(term_history[i], term_history[i+1]);
    str_copy(term_history[7], text);
}

void term_clear(void) {
    for (int i = 0; i < 8; i++) term_history[i][0] = '\0';
}

void execute_term_cmd(void) {
    char cmd[20] = "", arg[20] = "", arg2[20] = "";
    int i = 0, c = 0, a = 0, a2 = 0;
    while (term_input[i] && term_input[i] != ' ' && c < 19) cmd[c++] = term_input[i++];
    cmd[c] = '\0';
    if (term_input[i] == ' ') i++;
    while (term_input[i] && term_input[i] != ' ' && a < 19) arg[a++] = term_input[i++];
    arg[a] = '\0';
    while (term_input[i] == ' ') i++;
    while (term_input[i] && a2 < 19) arg2[a2++] = term_input[i++];
    arg2[a2] = '\0';

    if (str_cmp(cmd, "ls") == 0) {
        char list_buf[60] = "";
        int count = 0;
        for (int f = 0; f < FS_MAX_NODES; f++) {
            if (fs_table[f].is_used && fs_table[f].parent_id == 0) {
                if (count > 0) {
                    int l = str_len(list_buf);
                    if (l < 55) { list_buf[l] = ' '; list_buf[l+1] = '\0'; }
                }
                int l = str_len(list_buf);
                int nl = str_len(fs_table[f].name);
                if (l + nl < 58) {
                    str_copy(list_buf + l, fs_table[f].name);
                }
                count++;
            }
        }
        term_print(count == 0 ? "(Dizin bos)" : list_buf);
        sound_click();
    }
    else if (str_cmp(cmd, "touch") == 0 || str_cmp(cmd, "o") == 0) {
        if (str_len(arg) == 0) {
            term_print("Kullanim: touch <dosya_adi>");
            sound_error();
        } else {
            int slot = fs_create(0, arg, FS_TYPE_FILE, 20, 20);
            if (slot != -1) {
                term_print("Dosya olusturuldu.");
                sound_open();
            } else {
                term_print("Hata: Alan dolu!");
                sound_error();
            }
        }
    }
    else if (str_cmp(cmd, "mkdir") == 0 || str_cmp(cmd, "o.d") == 0) {
        if (str_len(arg) == 0) {
            term_print("Kullanim: mkdir <klasor_adi>");
            sound_error();
        } else {
            int slot = fs_create(0, arg, FS_TYPE_DIR, 20, 20);
            if (slot != -1) {
                term_print("Klasor olusturuldu.");
                sound_open();
            } else {
                term_print("Hata: Alan dolu!");
                sound_error();
            }
        }
    }
    else if (str_cmp(cmd, "cat") == 0) {
        int node = fs_find_child(0, arg);
        if (node != -1 && fs_table[node].type == FS_TYPE_FILE) {
            term_print(fs_table[node].size > 0 ? fs_table[node].content : "(Dosya bos)");
            sound_click();
        } else {
            term_print("Dosya bulunamadi!");
            sound_error();
        }
    }
    else if (str_cmp(cmd, "rm") == 0) {
        int node = fs_find_child(0, arg);
        if (node != -1) {
            fs_delete(node);
            term_print("Silindi.");
            sound_close();
        } else {
            term_print("Dosya bulunamadi!");
            sound_error();
        }
    }
    else if (str_cmp(cmd, "mv") == 0 || str_cmp(cmd, "tas") == 0) {
        int node = fs_find_child(0, arg);
        int target = fs_find_child(0, arg2);
        if (node != -1 && target != -1 && fs_table[target].type == FS_TYPE_DIR && fs_move(node, target)) {
            term_print("Dosya tasindi.");
            sound_open();
        } else {
            term_print("Kullanim: mv <dosya> <klasor>");
            sound_error();
        }
    }
    else if (str_cmp(cmd, "defter") == 0) { 
        int node = fs_find_child(0, arg);
        if (node != -1 && fs_table[node].type == FS_TYPE_FILE) {
            opened_file_id = node; w_not.open = 1; w_not.minimized = 0; focused_app = 1;
            bring_to_front(&w_not);
            sound_open();
        } else {
            term_print("Dosya bulunamadi!");
            sound_error();
        }
    }
    else if (str_cmp(cmd, "cu") == 0 || str_cmp(cmd, "calistir") == 0 || str_cmp(cmd, "run") == 0) {
        int node = fs_find_child(0, arg);
        if (node != -1 && fs_is_runnable(node)) {
            launch_file(node);
            term_print(fs_get_kind(node) == FS_KIND_SOURCE ?
                "C Programi Derlenip Calistirildi." : "EXC Programi Calistirildi.");
        } else if (node != -1 && fs_get_kind(node) == FS_KIND_LIBRARY) {
            term_print("LB kutuphanesi program degildir; C veya EXC icinden baglanir.");
        } else {
            term_print("Uygulama bulunamadi!");
            sound_error();
        }
    }
    else if (str_cmp(cmd, "clear") == 0) {
        term_clear();
        sound_click();
    }
    else if (str_cmp(cmd, "yardim") == 0 || str_cmp(cmd, "help") == 0) {
        term_print("Komutlar: ls, touch, mkdir, cat, rm, mv, defter, run, clear");
        sound_click();
    }
    else if (str_len(cmd) > 0) { 
        term_print("Bilinmeyen komut! 'yardim' yazin."); 
        sound_error();
    }

    term_input[0] = '\0'; term_input_len = 0;
}

static void handle_file_panel_key(char key) {
    if (file_panel_mode == 1) {
        if (key == '\n' || key == '\r') {
            if (file_panel_node != -1 && fs_rename(file_panel_node, file_panel_input)) {
                file_panel_open = 0;
                sound_open();
            } else {
                sound_error();
            }
        } else if (key == '\b' && file_panel_input_len > 0) {
            file_panel_input[--file_panel_input_len] = '\0';
        } else if (file_panel_input_len < FS_NAME_MAX - 1 && key >= 32 && key <= 126) {
            file_panel_input[file_panel_input_len++] = key;
            file_panel_input[file_panel_input_len] = '\0';
        }
    }
}

static void change_file_type(const char* extension) {
    if (file_panel_node == -1) return;
    char new_name[FS_NAME_MAX];
    copy_extension_name(new_name, fs_table[file_panel_node].name, extension);
    if (fs_rename(file_panel_node, new_name)) {
        file_panel_open = 0;
        sound_open();
    } else {
        sound_error();
    }
}

static void handle_file_panel_click(void) {
    if (!file_panel_open || !is_hover(170, 80, 300, 260)) {
        if (file_panel_open) file_panel_open = 0;
        return;
    }

    if (file_panel_mode == 1) {
        if (is_hover(190, 210, 120, 25)) handle_file_panel_key('\n');
        else if (is_hover(325, 210, 120, 25)) file_panel_open = 0;
    } else if (file_panel_mode == 2) {
        if (is_hover(190, 145, 75, 25)) change_file_type("c");
        else if (is_hover(275, 145, 75, 25)) change_file_type("exc");
        else if (is_hover(360, 145, 75, 25)) change_file_type("lb");
        else if (is_hover(325, 210, 120, 25)) file_panel_open = 0;
    } else if (file_panel_mode == 3) {
        if (is_hover(190, 145, 115, 25)) {
            if (fs_get_kind(file_panel_node) == FS_KIND_LIBRARY) {
                term_print("LB kutuphanesi dogrudan calistirilamaz.");
                open_terminal_window();
                file_panel_open = 0;
                return;
            }
            open_terminal_window();
            c_compile_and_run(fs_table[file_panel_node].content);
            focused_app = 3;
            bring_to_front(&w_trm);
            file_panel_open = 0;
        } else if (is_hover(315, 145, 115, 25)) {
            launch_file(file_panel_node);
            file_panel_open = 0;
        } else if (is_hover(240, 180, 115, 25)) {
            open_notepad_file(file_panel_node);
            file_panel_open = 0;
        } else if (is_hover(325, 210, 120, 25)) file_panel_open = 0;
    }
}

static void handle_notepad_key(int key) {
    notepad_clamp_cursor();
    if (key == KEY_LEFT) {
        if (notepad_cursor > 0) notepad_cursor--;
    } else if (key == KEY_RIGHT) {
        if (notepad_cursor < notepad_length()) notepad_cursor++;
    } else if (key == KEY_UP) {
        notepad_move_vertical(-1);
    } else if (key == KEY_DOWN) {
        notepad_move_vertical(1);
    } else if (key == '\b') {
        notepad_backspace();
    } else if (key == '\n' || key == '\r') {
        notepad_insert('\n');
    } else if (key >= 32 && key <= 126) {
        notepad_insert((char)key);
    }
}

static void set_notepad_cursor_from_mouse(void) {
    char* text = notepad_buffer();
    int target_row = (mouse_y - (w_not.y + 40)) / 18;
    int target_col = (mouse_x - (w_not.x + 15)) / 8;
    if (target_row < 0) target_row = 0;
    if (target_col < 0) target_col = 0;

    int row = 0;
    int index = 0;
    while (text[index] && row < target_row) {
        if (text[index++] == '\n') row++;
    }
    while (text[index] && text[index] != '\n' && target_col-- > 0) index++;
    notepad_cursor = index;
}

// --- HESAP MAKİNESİ ---
char notepad_temp[FS_FILE_MAX_SIZE] = "";
char calc_display[20] = "0"; int calc_state = 0, val1 = 0, val2 = 0; char operator = 0;
void calc_press(char key) {
    if (key == 'C') { val1 = val2 = calc_state = operator = 0; calc_display[0] = '0'; calc_display[1] = '\0'; return; }
    if (key >= '0' && key <= '9') { int v = key - '0'; if (calc_state == 0) { val1 = val1 * 10 + v; itoa(val1, calc_display); } else { val2 = val2 * 10 + v; itoa(val2, calc_display); } }
    else if (key == '+' || key == '-' || key == '*' || key == '/') { operator = key; calc_state = 1; val2 = 0; }
    else if (key == '=') { if (operator == '+') val1 += val2; if (operator == '-') val1 -= val2; if (operator == '*') val1 *= val2; if (operator == '/') { if(val2!=0) val1 /= val2; } itoa(val1, calc_display); calc_state = 0; }
}

static int prev_mouse_down = 0;
static int prev_right_mouse_down = 0;
int is_hover(int x, int y, int w, int h) { return (mouse_x >= x && mouse_x <= x + w && mouse_y >= y && mouse_y <= y + h); }

void handle_window_events(Window* w, int is_mouse_down, int mouse_pressed) {
    if (!w->open || w->minimized) return;

    if (mouse_pressed && is_hover(w->x, w->y, w->w, w->h)) {
        focused_app = w->id;
        bring_to_front(w);
    }
    if (mouse_pressed && is_hover(w->x + w->w - 15, w->y + w->h - 15, 15, 15)) { w->resizing = 1; }
    else if (mouse_pressed && is_hover(w->x, w->y, w->w - 90, 25)) { w->dragging = 1; w->drag_ox = mouse_x - w->x; w->drag_oy = mouse_y - w->y; }
    
    if (!is_mouse_down) { w->dragging = 0; w->resizing = 0; }
    if (w->dragging) { w->x = mouse_x - w->drag_ox; w->y = mouse_y - w->drag_oy; }
    
    if (w->resizing) {
        int new_w = mouse_x - w->x;
        int new_h = mouse_y - w->y;
        w->w = (new_w > 180) ? new_w : 180; 
        w->h = (new_h > 120) ? new_h : 120; 
    }

    if (mouse_pressed) {
        if (is_hover(w->x + w->w - 30, w->y, 30, 25) &&
            !(w->id == 0 && !welcome_complete)) { 
            w->open = 0; 
            sound_close(); 
        } 
        if (is_hover(w->x + w->w - 60, w->y, 30, 25)) { 
            if (w->maximized) { w->x = w->px; w->y = w->py; w->w = w->pw; w->h = w->ph; w->maximized = 0; }
            else { w->px = w->x; w->py = w->y; w->pw = w->w; w->ph = w->h; w->x = 0; w->y = 0; w->w = 640; w->h = 440; w->maximized = 1; }
            sound_click();
        }
        if (is_hover(w->x + w->w - 90, w->y, 30, 25)) { 
            w->minimized = 1; 
            sound_click();
        } 
    }
}

void create_new_lb_template(int target_dir) {
    char base_name[FS_NAME_MAX] = "yeni_app.lb";
    for (int k = 1; k < 9; k++) {
        if (fs_find_child(target_dir, base_name) == -1) break;
        base_name[8] = '0' + k;
    }
    int slot = fs_create(target_dir, base_name, FS_TYPE_FILE, 20, 20);
    if (slot != -1) {
        const char* template_code = 
            "# Ibrahim OS kutuphanesi\n"
            "# Donanim API: ekran, ses, klavye\n"
            "# Bu dosya .c ve .exc programlarina baglanir.\n";
        fs_write(slot, template_code, str_len(template_code));
        opened_file_id = slot;
        w_not.open = 1;
        w_not.minimized = 0;
        focused_app = 1;
        bring_to_front(&w_not);
        sound_open();
    }
}

void gui_process_clicks(void) {
    gui_frame_counter++;
    int mouse_down = left_clicked;
    int mouse_pressed = (mouse_down && !prev_mouse_down);
    prev_mouse_down = mouse_down;

    int right_pressed = (right_clicked && !prev_right_mouse_down);
    prev_right_mouse_down = right_clicked;

    // --- KLAVYE İŞLEMLERİ (ENTER VE METİN YÖNETİMİ) ---
    if (last_key_pressed) {
        if (file_panel_open) {
            handle_file_panel_key(last_key_pressed);
        }
        else if (focused_app == 0 && !welcome_complete) { // Hosgeldin
            if (last_key_pressed == '\n' || last_key_pressed == '\r') {
                if (welcome_name_len == 0) {
                    str_copy(welcome_name, "Kullanici");
                    welcome_name_len = str_len(welcome_name);
                }
                welcome_complete = 1;
            } else if (last_key_pressed == '\b' && welcome_name_len > 0) {
                welcome_name[--welcome_name_len] = '\0';
            } else if (welcome_name_len < 30 && last_key_pressed != '\b' &&
                       last_key_pressed >= 32 && last_key_pressed <= 126) {
                welcome_name[welcome_name_len++] = last_key_pressed;
                welcome_name[welcome_name_len] = '\0';
            }
        }
        else if (focused_app == 3) { // Terminal
            if (last_key_pressed == '\n' || last_key_pressed == '\r') {
                execute_term_cmd();
            }
            else if (last_key_pressed == '\b' && term_input_len > 0) {
                term_input[--term_input_len] = '\0';
            }
            else if (term_input_len < 38 && last_key_pressed != '\b') {
                term_input[term_input_len++] = last_key_pressed;
                term_input[term_input_len] = '\0';
            }
        }
        else if (focused_app == 1) { // Not Defteri
            handle_notepad_key(last_key_pressed);
        }
        last_key_pressed = 0;
    }

    if (file_panel_open && mouse_pressed) {
        handle_file_panel_click();
        return;
    }

    // --- İBRAHİM PAINT ÇİZİM DÖNGÜSÜ ---
    if (w_pnt.open && !w_pnt.minimized && mouse_down) {
        int cvs_x = w_pnt.x + 10;
        int cvs_y = w_pnt.y + 65;
        if (mouse_x >= cvs_x && mouse_x < cvs_x + PAINT_W && mouse_y >= cvs_y && mouse_y < cvs_y + PAINT_H) {
            int lx = mouse_x - cvs_x;
            int ly = mouse_y - cvs_y;
            for (int dy = 0; dy < paint_brush_size; dy++) {
                for (int dx = 0; dx < paint_brush_size; dx++) {
                    if (lx + dx < PAINT_W && ly + dy < PAINT_H) {
                        paint_canvas[(ly + dy) * PAINT_W + (lx + dx)] = current_paint_color;
                    }
                }
            }
        }
    }

    // --- PENCERE ETKİLEŞİMLERİ ---
    for (int i = WINDOW_COUNT - 1; i >= 0; i--) {
        handle_window_events(window_stack[i], mouse_down, mouse_pressed);
    }

    if (w_not.open && !w_not.minimized && mouse_pressed &&
        is_hover(w_not.x + 10, w_not.y + 35, w_not.w - 20, w_not.h - 45)) {
        focused_app = 1;
        set_notepad_cursor_from_mouse();
    }

    // --- LB APP BUTON TIKLAMALARI ---
    if (w_app.open && !w_app.minimized && mouse_pressed) {
        cu_handle_click(w_app.x, w_app.y, mouse_x, mouse_y);
    }

    // --- SAĞ TIK MENÜSÜ AÇMA ---
    if (right_pressed) {
        ctx_menu_open = 1;
        ctx_menu_x = mouse_x;
        ctx_menu_y = mouse_y;
        ctx_target_node = -1;
        start_menu_open = 0;
        sound_click();

        for (int f = 0; f < FS_MAX_NODES; f++) {
            if (fs_table[f].is_used && fs_table[f].parent_id == 0 &&
                is_hover(fs_table[f].x, fs_table[f].y, 40, 40)) {
                ctx_target_node = f;
                break;
            }
        }
        
        if (w_exp.open && !w_exp.minimized && is_hover(w_exp.x, w_exp.y, w_exp.w, w_exp.h)) {
            ctx_target_dir = current_exp_dir;
            int exp_count = 0;
            for (int f = 0; f < FS_MAX_NODES; f++) {
                if (fs_table[f].is_used && fs_table[f].parent_id == current_exp_dir) {
                    int local_x = w_exp.x + 20 + (exp_count % 4) * 80;
                    int local_y = w_exp.y + 60 + (exp_count / 4) * 70;
                    if (is_hover(local_x, local_y, 40, 40)) {
                        ctx_target_node = f;
                        break;
                    }
                    exp_count++;
                }
            }
        } else {
            ctx_target_dir = 0;
        }
    }

    // --- SİMGELERİ SÜRÜKLEME ---
    if (mouse_pressed && dragging_file_id == -1 && !ctx_menu_open && !start_menu_open) {
        for(int f=0; f<FS_MAX_NODES; f++) {
            if(fs_table[f].is_used && fs_table[f].parent_id == 0 && is_hover(fs_table[f].x, fs_table[f].y, 40, 40)) {
                dragging_file_id = f;
                drag_f_ox = mouse_x - fs_table[f].x;
                drag_f_oy = mouse_y - fs_table[f].y;
                sound_click();
                break;
            }
        }
    }
    if (!mouse_down && dragging_file_id != -1) {
        for (int f = 0; f < FS_MAX_NODES; f++) {
            if (fs_table[f].is_used && fs_table[f].type == FS_TYPE_DIR &&
                fs_table[f].parent_id == 0 && f != dragging_file_id &&
                is_hover(fs_table[f].x, fs_table[f].y, 40, 40)) {
                if (fs_move(dragging_file_id, f)) sound_open();
                break;
            }
        }
        dragging_file_id = -1;
    }
    if (dragging_file_id != -1) {
        fs_table[dragging_file_id].x = mouse_x - drag_f_ox;
        fs_table[dragging_file_id].y = mouse_y - drag_f_oy;
    }

    if (!mouse_pressed) return; 

    // --- BAŞLAT MENÜSÜ TIKLAMALARI ---
    if (start_menu_open) {
        int sm_y = 440 - 270;
        if (is_hover(0, sm_y, 160, 270)) {
            if (is_hover(10, sm_y + 10, 140, 25))  { w_not.open = 1; w_not.minimized = 0; opened_file_id = -1; focused_app = 1; bring_to_front(&w_not); sound_open(); }
            if (is_hover(10, sm_y + 40, 140, 25))  { w_cal.open = 1; w_cal.minimized = 0; focused_app = 2; bring_to_front(&w_cal); sound_open(); }
            if (is_hover(10, sm_y + 70, 140, 25))  { w_exp.open = 1; w_exp.minimized = 0; focused_app = 4; bring_to_front(&w_exp); sound_open(); }
            if (is_hover(10, sm_y + 100, 140, 25)) { w_pnt.open = 1; w_pnt.minimized = 0; focused_app = 5; bring_to_front(&w_pnt); sound_open(); }
            if (is_hover(10, sm_y + 130, 140, 25)) { create_new_lb_template(0); } // LB Yapici
            if (is_hover(10, sm_y + 160, 140, 25)) { w_trm.open = 1; w_trm.minimized = 0; focused_app = 3; bring_to_front(&w_trm); sound_open(); }
            if (is_hover(10, sm_y + 190, 140, 25)) { w_set.open = 1; w_set.minimized = 0; focused_app = 6; bring_to_front(&w_set); sound_open(); }
            if (is_hover(10, sm_y + 225, 140, 25)) { sound_close(); sleep_ms(100); outw(0x604, 0x2000); }
            start_menu_open = 0;
            return;
        } else if (!is_hover(5, 444, 70, 32)) {
            start_menu_open = 0;
        }
    }

    // --- SAĞ TIK MENÜSÜ İŞLEMLERİ ---
    if (ctx_menu_open) {
        if (ctx_target_node != -1) {
            if (is_hover(ctx_menu_x, ctx_menu_y, 180, 25)) {
                open_file_panel(ctx_target_node, 1);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 25, 180, 25)) {
                open_file_panel(ctx_target_node, 2);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 50, 180, 25)) {
                if (fs_is_editable(ctx_target_node)) open_notepad_file(ctx_target_node);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 75, 180, 25)) {
                open_file_panel(ctx_target_node, 3);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 100, 180, 25)) {
                convert_file(ctx_target_node, "exc", 0);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 125, 180, 25)) {
                convert_file(ctx_target_node, "exc", 1);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 150, 180, 25)) {
                convert_file(ctx_target_node, "o", 0);
            } else if (is_hover(ctx_menu_x, ctx_menu_y + 175, 180, 25)) {
                fs_delete(ctx_target_node);
                ctx_target_node = -1;
                sound_close();
            }
        } else if (is_hover(ctx_menu_x, ctx_menu_y, 180, 25)) {
            char base_name[FS_NAME_MAX] = "Klasor";
            for (int k = 0; k < 9; k++) {
                if (k > 0) { base_name[6] = '('; base_name[7] = '0' + k; base_name[8] = ')'; base_name[9] = '\0'; }
                if (fs_find_child(ctx_target_dir, base_name) == -1) break;
            }
            int px = (ctx_target_dir == 0) ? ctx_menu_x : 0;
            int py = (ctx_target_dir == 0) ? ctx_menu_y : 0;
            fs_create(ctx_target_dir, base_name, FS_TYPE_DIR, px, py);
            sound_open();
        }
        else if (is_hover(ctx_menu_x, ctx_menu_y + 25, 180, 25)) {
            create_new_lb_template(ctx_target_dir);
        } else if (is_hover(ctx_menu_x, ctx_menu_y + 50, 180, 25)) {
            create_source_file(ctx_target_dir, "c");
        } else if (is_hover(ctx_menu_x, ctx_menu_y + 75, 180, 25)) {
            create_source_file(ctx_target_dir, "exc");
        }
        ctx_menu_open = 0; 
        return; 
    }

    // --- MASAÜSTÜ SİMGELERİNE TIKLAMA ---
    for(int f=0; f<FS_MAX_NODES; f++) {
        if(fs_table[f].is_used && fs_table[f].parent_id == 0 && is_hover(fs_table[f].x, fs_table[f].y, 40, 40)) {
            if (!is_double_click(f)) return;
            if (fs_table[f].type == FS_TYPE_DIR) { 
                w_exp.open = 1; w_exp.minimized = 0; focused_app = 4; current_exp_dir = f; bring_to_front(&w_exp);
            } else if (fs_is_runnable(f)) {
                launch_file(f);
            } else { 
                opened_file_id = f; w_not.open = 1; w_not.minimized = 0; focused_app = 1; bring_to_front(&w_not);
            }
            sound_open();
            return;
        }
    }

    // --- DOSYA GEZGİNİ TIKLAMALARI ---
    if (w_exp.open && !w_exp.minimized && is_hover(w_exp.x, w_exp.y, w_exp.w, w_exp.h)) {
        if (current_exp_dir != 0 && is_hover(w_exp.x + 10, w_exp.y + 35, 60, 20)) {
            current_exp_dir = fs_table[current_exp_dir].parent_id;
            sound_click();
            return;
        }
        int exp_count = 0;
        for(int f=0; f<FS_MAX_NODES; f++) {
            if(fs_table[f].is_used && fs_table[f].parent_id == current_exp_dir) {
                int local_x = w_exp.x + 20 + (exp_count % 4) * 80;
                int local_y = w_exp.y + 60 + (exp_count / 4) * 70;
                if (is_hover(local_x, local_y, 40, 40)) {
                    if (!is_double_click(f)) return;
                    if (fs_table[f].type == FS_TYPE_DIR) { 
                        current_exp_dir = f; 
                    } else if (fs_is_runnable(f)) {
                        launch_file(f);
                    } else { 
                        opened_file_id = f; w_not.open = 1; w_not.minimized = 0; focused_app = 1; bring_to_front(&w_not); 
                    }
                    sound_open();
                    return;
                }
                exp_count++;
            }
        }
    }

    // --- AYARLAR PENCERESİ TIKLAMALARI ---
    if (w_set.open && !w_set.minimized) {
        if (is_hover(w_set.x + 20,  w_set.y + 55, 45, 22)) { desktop_wallpaper = 0; desktop_bg_color = 0x0078D7; sound_click(); }
        if (is_hover(w_set.x + 75,  w_set.y + 55, 45, 22)) { desktop_wallpaper = 1; desktop_bg_color = 0x008080; sound_click(); }
        if (is_hover(w_set.x + 130, w_set.y + 55, 45, 22)) { desktop_wallpaper = 2; desktop_bg_color = 0x123047; sound_click(); }
        if (is_hover(w_set.x + 185, w_set.y + 55, 45, 22)) { desktop_wallpaper = 3; desktop_bg_color = 0x24113A; sound_click(); }

        if (is_hover(w_set.x + 20, w_set.y + 105, 120, 24)) {
            sound_enabled = !sound_enabled;
            if (sound_enabled) sound_open();
        }
        if (is_hover(w_set.x + 155, w_set.y + 105, 150, 24)) {
            term_clear();
            term_print("Terminal temizlendi.");
            sound_click();
        }
        if (is_hover(w_set.x + 155, w_set.y + 135, 150, 24)) {
            desktop_wallpaper = 0;
            desktop_bg_color = 0x0078D7;
            sound_open();
        }
    }

    // --- İBRAHİM PAINT KONTROLLERİ ---
    if (w_pnt.open && !w_pnt.minimized) {
        uint32_t colors[6] = {0x000000, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFFFFFF};
        for (int i = 0; i < 6; i++) {
            if (is_hover(w_pnt.x + 15 + (i * 24), w_pnt.y + 35, 20, 20)) {
                current_paint_color = colors[i];
                sound_click();
            }
        }
        if (is_hover(w_pnt.x + 170, w_pnt.y + 35, 50, 20)) {
            paint_brush_size = (paint_brush_size == 1) ? 3 : 1;
            sound_click();
        }
        if (is_hover(w_pnt.x + 230, w_pnt.y + 35, 60, 20)) {
            paint_init();
            sound_close();
        }
    }

    // --- GÖREV ÇUBUĞU BUTONLARI ---
    if (is_hover(5, 444, 70, 32)) { start_menu_open = !start_menu_open; sound_click(); }
    if (is_hover(85, 445, 38, 30)) { w_trm.open = 1; w_trm.minimized = 0; focused_app = 3; bring_to_front(&w_trm); sound_open(); } 
    if (is_hover(128, 445, 38, 30)) { w_not.open = 1; w_not.minimized = 0; opened_file_id = -1; focused_app = 1; bring_to_front(&w_not); sound_open(); } 
    if (is_hover(171, 445, 38, 30)) { w_cal.open = 1; w_cal.minimized = 0; focused_app = 2; bring_to_front(&w_cal); sound_open(); } 
    if (is_hover(214, 445, 38, 30)) { w_exp.open = 1; w_exp.minimized = 0; focused_app = 4; bring_to_front(&w_exp); sound_open(); } 
    if (is_hover(257, 445, 38, 30)) { w_pnt.open = 1; w_pnt.minimized = 0; focused_app = 5; bring_to_front(&w_pnt); sound_open(); } 
    if (is_hover(300, 445, 38, 30)) { w_set.open = 1; w_set.minimized = 0; focused_app = 6; bring_to_front(&w_set); sound_open(); } 
    if (is_hover(585, 445, 50, 30)) { sound_close(); sleep_ms(100); outw(0x604, 0x2000); }

    // Hesap Makinesi Tuşları
    if (w_cal.open && !w_cal.minimized) {
        char keys[16] = {'7','8','9','/','4','5','6','*','1','2','3','-','C','0','=','+'};
        for(int r=0; r<4; r++) {
            for(int c=0; c<4; c++) {
                if (is_hover(w_cal.x+20+(c*40), w_cal.y+90+(r*30), 30, 20)) {
                    calc_press(keys[r*4 + c]);
                    sound_click();
                }
            }
        }
    }
}

// --- PENCERE ÇİZİM YARDIMCISI ---
void draw_modern_window(Window* w, const char* title) {
    if (!w->open || w->minimized) return;
    draw_rect(w->x, w->y, w->w, w->h, 0xF0F0F0); 
    draw_rect(w->x, w->y, w->w, 25, (focused_app == w->id) ? 0x0055AA : 0x777777); 
    draw_string(title, w->x + 10, w->y + 5, COLOR_WHITE);

    draw_rect(w->x + w->w - 90, w->y, 30, 25, is_hover(w->x + w->w - 90, w->y, 30, 25) ? 0x999999 : 0x777777); draw_string("-", w->x + w->w - 80, w->y + 5, COLOR_WHITE);
    draw_rect(w->x + w->w - 60, w->y, 30, 25, is_hover(w->x + w->w - 60, w->y, 30, 25) ? 0x999999 : 0x777777); draw_rect(w->x + w->w - 50, w->y + 7, 10, 10, COLOR_WHITE); draw_rect(w->x + w->w - 48, w->y + 9, 6, 6, 0x777777);
    draw_rect(w->x + w->w - 30, w->y, 30, 25, is_hover(w->x + w->w - 30, w->y, 30, 25) ? COLOR_RED : 0x777777); draw_string("X", w->x + w->w - 20, w->y + 5, COLOR_WHITE);
    draw_rect(w->x + w->w - 10, w->y + w->h - 10, 10, 10, 0xAAAAAA);
}

static void draw_desktop_background(void) {
    draw_rect(0, 0, 640, 480, desktop_bg_color);
    if (desktop_wallpaper == 1) {
        for (int y = 0; y < 440; y += 24) draw_rect(0, y, 640, 2, 0x16A085);
    } else if (desktop_wallpaper == 2) {
        for (int x = 0; x < 640; x += 32) draw_rect(x, 0, 1, 440, 0x1D4D68);
        for (int y = 0; y < 440; y += 32) draw_rect(0, y, 640, 1, 0x1D4D68);
    } else if (desktop_wallpaper == 3) {
        for (int offset = -440; offset < 640; offset += 28) {
            for (int y = 0; y < 440; y += 4) {
                int x = offset + y;
                if (x >= 0 && x < 640) put_pixel(x, y, 0x54247A);
            }
        }
    }
}

static void draw_notepad_content(void) {
    char* text = notepad_buffer();
    int x = w_not.x + 15;
    int draw_x = x;
    int draw_y = w_not.y + 40;
    int length = notepad_length();
    for (int i = 0; i <= length; i++) {
        if (i == notepad_cursor && (gui_frame_counter % 60) < 30) {
            draw_rect(draw_x, draw_y, 2, 16, COLOR_BLACK);
        }
        if (i == length) break;
        if (text[i] == '\n') {
            draw_x = x;
            draw_y += 18;
        } else {
            draw_char(text[i], draw_x, draw_y, COLOR_BLACK);
            draw_x += 8;
        }
    }
}

void gui_draw(void) {
    draw_desktop_background();

    // --- MASAÜSTÜ SİMGELERİ ---
    for(int f=0; f<FS_MAX_NODES; f++) {
        if(fs_table[f].is_used && fs_table[f].parent_id == 0) {
            uint32_t color = (fs_table[f].type == FS_TYPE_DIR) ? 0xF1C40F :
                (fs_get_kind(f) == FS_KIND_LIBRARY ? 0x3498DB :
                (fs_is_runnable(f) ? 0x9B59B6 : 0xEEEEEE));
            draw_rect(fs_table[f].x, fs_table[f].y, 30, 35, color);
            draw_rect(fs_table[f].x, fs_table[f].y, 15, 5, color);
            draw_string(fs_table[f].name, fs_table[f].x-10, fs_table[f].y+40, COLOR_WHITE); 
        }
    }

    // --- PENCERELERİ Z-INDEX SIRASIYLA ÇİZ ---
    for (int i = 0; i < WINDOW_COUNT; i++) {
        Window* w = window_stack[i];
        if (!w->open || w->minimized) continue;

        if (w->id == 0) { // Bilgi
            draw_modern_window(&w_wel, "Hosgeldin");
            draw_string("Esselamualeykum!", w_wel.x + 35, w_wel.y + 40, COLOR_BLACK);
            if (!welcome_complete) {
                draw_string("Kullanici adiniz:", w_wel.x + 20, w_wel.y + 62, COLOR_BLACK);
                draw_rect(w_wel.x + 20, w_wel.y + 80, 240, 22, COLOR_WHITE);
                draw_string(welcome_name, w_wel.x + 25, w_wel.y + 83, COLOR_BLACK);
                draw_string("Enter ile devam", w_wel.x + 20, w_wel.y + 108, 0x555555);
            } else {
                draw_string("Sistem kuruldu!", w_wel.x + 20, w_wel.y + 65, COLOR_BLACK);
                draw_string("Iste", w_wel.x + 20, w_wel.y + 87, COLOR_BLACK);
                draw_string(welcome_name, w_wel.x + 55, w_wel.y + 87, 0x0055AA);
                draw_string("gucu!", w_wel.x + 65 + str_len(welcome_name) * 8,
                            w_wel.y + 87, COLOR_BLACK);
            }
        }
        else if (w->id == 1) { // Not Defteri
            draw_modern_window(&w_not, "Not Defteri");
            draw_rect(w_not.x+10, w_not.y+35, w_not.w-20, w_not.h-45, COLOR_WHITE);
            draw_notepad_content();
        }
        else if (w->id == 2) { // Hesap Makinesi
            draw_modern_window(&w_cal, "Hesap Makinesi");
            draw_rect(w_cal.x+20, w_cal.y+40, 150, 30, COLOR_WHITE); draw_string(calc_display, w_cal.x+30, w_cal.y+45, COLOR_BLACK);
            char keys[16] = {'7','8','9','/','4','5','6','*','1','2','3','-','C','0','=','+'};
            for(int r=0; r<4; r++) for(int c=0; c<4; c++) { draw_rect(w_cal.x+20+(c*40), w_cal.y+90+(r*30), 30, 20, 0xCCCCCC); draw_char(keys[r*4 + c], w_cal.x+20+(c*40)+10, w_cal.y+90+(r*30)+5, COLOR_BLACK); }
        }
        else if (w->id == 3) { // Terminal
            draw_modern_window(&w_trm, "Terminal");
            draw_rect(w_trm.x, w_trm.y+25, w_trm.w, w_trm.h-25, 0x111111); 
            for (int ti=0; ti<7; ti++) draw_string(term_history[ti], w_trm.x+10, w_trm.y+30+(ti*20), COLOR_WHITE);
            char prompt[30] = "ibrahimos$ ";
            draw_string(prompt, w_trm.x+10, w_trm.y+(w_trm.h-30), 0x00FF00); 
            draw_string(term_input, w_trm.x+10+(str_len(prompt)*8), w_trm.y+(w_trm.h-30), COLOR_WHITE);
        }
        else if (w->id == 4) { // Gezgin
            char exp_title[35] = "Gezgin - ";
            str_copy(exp_title + 9, (current_exp_dir == 0) ? "Masaustu" : fs_table[current_exp_dir].name);
            draw_modern_window(&w_exp, exp_title);
            draw_rect(w_exp.x + 5, w_exp.y + 60, w_exp.w - 10, w_exp.h - 65, COLOR_WHITE);

            if (current_exp_dir != 0) {
                draw_rect(w_exp.x + 10, w_exp.y + 35, 60, 20, is_hover(w_exp.x+10, w_exp.y+35, 60, 20) ? 0xCCCCCC : 0xDDDDDD);
                draw_string("< Geri", w_exp.x + 15, w_exp.y + 37, COLOR_BLACK);
            }

            int exp_count = 0;
            for(int f=0; f<FS_MAX_NODES; f++) {
                if(fs_table[f].is_used && fs_table[f].parent_id == current_exp_dir) {
                    int local_x = w_exp.x + 20 + (exp_count % 4) * 80;
                    int local_y = w_exp.y + 70 + (exp_count / 4) * 70;
                    uint32_t color = (fs_table[f].type == FS_TYPE_DIR) ? 0xF1C40F :
                        (fs_get_kind(f) == FS_KIND_LIBRARY ? 0x3498DB :
                        (fs_is_runnable(f) ? 0x9B59B6 : 0xEEEEEE));
                    draw_rect(local_x, local_y, 30, 35, color);
                    draw_rect(local_x, local_y, 15, 5, color);
                    draw_string(fs_table[f].name, local_x-10, local_y+40, COLOR_BLACK);
                    exp_count++;
                }
            }
        }
        else if (w->id == 5) { // İbrahim Paint
            draw_modern_window(&w_pnt, "Ibrahim Paint");
            
            uint32_t pal[6] = {0x000000, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFFFFFF};
            for (int ci = 0; ci < 6; ci++) {
                draw_rect(w_pnt.x + 15 + (ci * 24), w_pnt.y + 35, 20, 20, pal[ci]);
                if (current_paint_color == pal[ci]) {
                    draw_rect(w_pnt.x + 15 + (ci * 24) + 6, w_pnt.y + 41, 8, 8, (pal[ci] == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE);
                }
            }
            draw_rect(w_pnt.x + 170, w_pnt.y + 35, 50, 20, 0xDDDDDD);
            draw_string(paint_brush_size == 1 ? "1px" : "3px", w_pnt.x + 180, w_pnt.y + 38, COLOR_BLACK);

            draw_rect(w_pnt.x + 230, w_pnt.y + 35, 60, 20, 0xDDDDDD);
            draw_string("Temizle", w_pnt.x + 235, w_pnt.y + 38, COLOR_BLACK);

            int cvs_x = w_pnt.x + 10;
            int cvs_y = w_pnt.y + 65;
            for (int cy = 0; cy < PAINT_H; cy++) {
                for (int cx = 0; cx < PAINT_W; cx++) {
                    put_pixel(cvs_x + cx, cvs_y + cy, paint_canvas[cy * PAINT_W + cx]);
                }
            }
        }
        else if (w->id == 6) { // Ayarlar
            draw_modern_window(&w_set, "Ayarlar & Sistem Bilgisi");
            
            draw_string("Masaustu Temasi:", w_set.x + 15, w_set.y + 35, COLOR_BLACK);
            draw_rect(w_set.x + 15,  w_set.y + 55, 45, 22, 0x0078D7); draw_string("Mavi",   w_set.x + 20,  w_set.y + 58, COLOR_WHITE);
            draw_rect(w_set.x + 65,  w_set.y + 55, 45, 22, 0x008080); draw_string("Cizgi",  w_set.x + 68,  w_set.y + 58, COLOR_WHITE);
            draw_rect(w_set.x + 115, w_set.y + 55, 45, 22, 0x123047); draw_string("Izgara", w_set.x + 117, w_set.y + 58, COLOR_WHITE);
            draw_rect(w_set.x + 165, w_set.y + 55, 45, 22, 0x24113A); draw_string("Gece",  w_set.x + 170, w_set.y + 58, COLOR_WHITE);

            draw_string("Ses Durumu:", w_set.x + 15, w_set.y + 85, COLOR_BLACK);
            draw_rect(w_set.x + 15, w_set.y + 105, 120, 24, sound_enabled ? 0x27AE60 : 0xC0392B);
            draw_string(sound_enabled ? "[X] Ses Acik" : "[ ] Ses Kapali", w_set.x + 20, w_set.y + 110, COLOR_WHITE);
            draw_rect(w_set.x + 155, w_set.y + 105, 150, 24, 0x34495E);
            draw_string("Terminali Temizle", w_set.x + 165, w_set.y + 110, COLOR_WHITE);
            draw_rect(w_set.x + 155, w_set.y + 135, 150, 24, 0x5D6D7E);
            draw_string("Temayi Sifirla", w_set.x + 170, w_set.y + 140, COLOR_WHITE);

            draw_rect(w_set.x + 15, w_set.y + 140, w_set.w - 30, 155, 0xE5E5E5);
            draw_rect(w_set.x + 15, w_set.y + 140, w_set.w - 30, 20, 0xCCCCCC);
            draw_string("Donanim & Sistem Bilgileri", w_set.x + 22, w_set.y + 143, COLOR_BLACK);

            draw_string("Isletim Sistemi: Ibrahim OS v2.5", w_set.x + 22, w_set.y + 165, 0x222222);
            draw_string("Mimari: x86 32-bit Protected Mode", w_set.x + 22, w_set.y + 185, 0x222222);

            char ram_str[35] = "Bellek (RAM): ";
            char ram_num[10];
            itoa((int)total_ram_mb, ram_num);
            str_copy(ram_str + 14, ram_num);
            int rl = str_len(ram_str);
            str_copy(ram_str + rl, " MB Aktif");
            draw_string(ram_str, w_set.x + 22, w_set.y + 205, 0x222222);

            draw_string("Ekran: 640x480x32 VBE TrueColor", w_set.x + 22, w_set.y + 225, 0x222222);

            int file_cnt = 0;
            for(int f=0; f<FS_MAX_NODES; f++) if (fs_table[f].is_used) file_cnt++;
            char fs_info[40] = "Depolama (VFS): ";
            char f_num[10];
            itoa(file_cnt, f_num);
            str_copy(fs_info + 16, f_num);
            int fl = str_len(fs_info);
            str_copy(fs_info + fl, " / 64 Node");
            draw_string(fs_info, w_set.x + 22, w_set.y + 245, 0x222222);

            draw_string("Suruculer: PS/2, RTC, PIT, LB kutuphaneleri", w_set.x + 22, w_set.y + 265, 0x0055AA);
        }
        else if (w->id == 7) { // LB Runtime Window
            draw_modern_window(&w_app, current_cu_app.title);
            cu_draw_app(w_app.x, w_app.y, w_app.w, w_app.h);
        }
    }

    // --- BAŞLAT MENÜSÜ ---
    if (start_menu_open) {
        int sm_y = 440 - 270;
        draw_rect(0, sm_y, 160, 270, 0x2A2A2A);
        draw_rect(0, sm_y, 25, 270, 0x8E44AD); // LB uygulama seridi
        
        char* items[8] = {"Not Defteri", "Hesap Makinesi", "Dosya Gezgini", "Ibrahim Paint", "LB Yapici (+)", "Terminal", "Ayarlar", "Sistemi Kapat"};
        for (int mi = 0; mi < 8; mi++) {
            int iy = sm_y + 10 + (mi * 30) + (mi == 7 ? 15 : 0);
            if (mi == 7) draw_rect(30, iy - 8, 125, 2, 0x555555);
            int hov = is_hover(30, iy, 125, 25);
            draw_rect(30, iy, 125, 25, hov ? (mi == 7 ? COLOR_RED : 0x8E44AD) : 0x2A2A2A);
            draw_string(items[mi], 35, iy + 5, COLOR_WHITE);
        }
    }

    // --- SAĞ TIK MENÜSÜ ---
    if (ctx_menu_open) {
        draw_rect(ctx_menu_x, ctx_menu_y, 140, 50, COLOR_WHITE);
        int menu_height = ctx_target_node == -1 ? 100 : 200;
        draw_rect(ctx_menu_x, ctx_menu_y, 180, menu_height, COLOR_WHITE);
        if (ctx_target_node == -1) {
            draw_string("Yeni Klasor", ctx_menu_x + 10, ctx_menu_y + 8, COLOR_BLACK);
            draw_string("Yeni LB App", ctx_menu_x + 10, ctx_menu_y + 33, 0x8E44AD);
            draw_string("Yeni C Dosyasi", ctx_menu_x + 10, ctx_menu_y + 58, COLOR_BLACK);
            draw_string("Yeni EXC Dosyasi", ctx_menu_x + 10, ctx_menu_y + 83, 0x9B59B6);
        } else {
            draw_string("Yeniden Adlandir", ctx_menu_x + 10, ctx_menu_y + 8, COLOR_BLACK);
            draw_string("Dosya Turunu Degistir", ctx_menu_x + 10, ctx_menu_y + 33, COLOR_BLACK);
            draw_string("Not Defteri ile Ac", ctx_menu_x + 10, ctx_menu_y + 58, COLOR_BLACK);
            draw_string("Baska Uygulama ile Ac", ctx_menu_x + 10, ctx_menu_y + 83, 0x8E44AD);
            draw_string("Acik EXC'ye Donustur", ctx_menu_x + 10, ctx_menu_y + 108, 0x9B59B6);
            draw_string("Kapali EXC'ye Donustur", ctx_menu_x + 10, ctx_menu_y + 133, 0x8E44AD);
            draw_string("O Nesnesine Donustur", ctx_menu_x + 10, ctx_menu_y + 158, COLOR_BLACK);
            draw_string("Sil", ctx_menu_x + 10, ctx_menu_y + 183, COLOR_RED);
        }
    }

    if (file_panel_open) {
        draw_rect(170, 80, 300, 260, 0xF0F0F0);
        draw_rect(170, 80, 300, 25, 0x0055AA);
        if (file_panel_mode == 1) {
            draw_string("Yeniden Adlandir", 180, 85, COLOR_WHITE);
            draw_string("Yeni isim:", 190, 125, COLOR_BLACK);
            draw_rect(190, 155, 255, 25, COLOR_WHITE);
            draw_string(file_panel_input, 195, 160, COLOR_BLACK);
            draw_string("Enter ile kaydet", 190, 190, 0x555555);
            draw_string("Kaydet", 205, 215, COLOR_WHITE);
            draw_string("Iptal", 355, 215, COLOR_WHITE);
        } else if (file_panel_mode == 2) {
            draw_string("Dosya Turunu Degistir", 180, 85, COLOR_WHITE);
            draw_string("Yeni turu secin:", 190, 125, COLOR_BLACK);
            draw_string("C", 220, 150, COLOR_WHITE);
            draw_string("EXC", 295, 150, COLOR_WHITE);
            draw_string("LB", 385, 150, COLOR_WHITE);
            draw_string("Iptal", 355, 215, COLOR_WHITE);
        } else {
            draw_string("Baska Uygulama ile Ac", 180, 85, COLOR_WHITE);
            draw_string("C Derleyicisi", 200, 150, COLOR_WHITE);
            draw_string("EXC Programi", 325, 150, COLOR_WHITE);
            draw_string("Not Defteri", 250, 185, COLOR_WHITE);
            draw_string("Iptal", 355, 215, COLOR_WHITE);
        }
        draw_rect(190, 145, 115, 25, 0x3498DB);
        draw_rect(315, 145, 115, 25, 0x9B59B6);
        if (file_panel_mode == 1) {
            draw_rect(190, 210, 120, 25, 0x27AE60);
            draw_rect(325, 210, 120, 25, 0xC0392B);
        } else if (file_panel_mode == 2) {
            draw_rect(360, 145, 75, 25, 0x2ECC71);
            draw_rect(325, 210, 120, 25, 0xC0392B);
        } else {
            draw_rect(240, 180, 115, 25, 0x34495E);
            draw_rect(325, 210, 120, 25, 0xC0392B);
        }
    }

    // --- GÖREV ÇUBUĞU ---
    draw_rect(0, 440, 640, 40, 0x1E1E1E);
    draw_rect(0, 440, 640, 2, 0x333333);

    draw_rect(5, 444, 70, 32, start_menu_open ? 0x5B2C6F : (is_hover(5, 444, 70, 32) ? 0x8E44AD : 0x6C3483));
    draw_string("Baslat", 15, 452, COLOR_WHITE);

    draw_rect(85, 445, 38, 30, w_trm.open ? 0x555555 : 0x333333); draw_string("Trm", 90, 455, COLOR_WHITE);
    draw_rect(128, 445, 38, 30, w_not.open ? 0x555555 : 0x333333); draw_string("Not", 133, 455, COLOR_WHITE);
    draw_rect(171, 445, 38, 30, w_cal.open ? 0x555555 : 0x333333); draw_string("Hsp", 176, 455, COLOR_WHITE);
    draw_rect(214, 445, 38, 30, w_exp.open ? 0x555555 : 0x333333); draw_string("Gzg", 219, 455, COLOR_WHITE);
    draw_rect(257, 445, 38, 30, w_pnt.open ? 0x555555 : 0x333333); draw_string("Pnt", 262, 455, COLOR_WHITE);
    draw_rect(300, 445, 38, 30, w_set.open ? 0x555555 : 0x333333); draw_string("Ayr", 305, 455, COLOR_WHITE);
    if (w_app.open) {
        draw_rect(343, 445, 38, 30, 0x8E44AD); draw_string(".LB", 348, 455, COLOR_WHITE);
    }

    char time_str[10];
    get_rtc_time_str(time_str);
    draw_rect(500, 445, 75, 30, 0x111111);
    draw_string(time_str, 506, 454, 0x00FF66);

    draw_rect(585, 445, 50, 30, is_hover(585,445,50,30) ? COLOR_RED : 0x333333);
    draw_string("Kpt", 597, 455, COLOR_WHITE);

    draw_mouse(mouse_x, mouse_y);
}
