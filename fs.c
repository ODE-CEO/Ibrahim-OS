#include "fs.h"

fs_node_t fs_table[FS_MAX_NODES];

static int fs_has_suffix(const char* name, const char* suffix) {
    int name_len = str_len(name);
    int suffix_len = str_len(suffix);
    if (name_len < suffix_len) return 0;
    return str_cmp(name + name_len - suffix_len, suffix) == 0;
}

static int fs_kind_for_name(const char* name) {
    if (fs_has_suffix(name, ".c")) return FS_KIND_SOURCE;
    if (fs_has_suffix(name, ".lb")) return FS_KIND_LIBRARY;
    if (fs_has_suffix(name, ".o")) return FS_KIND_OBJECT;
    if (fs_has_suffix(name, ".exc")) return FS_KIND_EXECUTABLE;
    return FS_KIND_TEXT;
}

void fs_init(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        fs_table[i].is_used = 0;
        fs_table[i].size = 0;
        fs_table[i].content[0] = '\0';
        fs_table[i].name[0] = '\0';
        fs_table[i].kind = FS_KIND_TEXT;
    }

    int readme = fs_create(0, "BeniOku.txt", FS_TYPE_FILE, 20, 20);
    if (readme != -1) {
        const char* msg = "Ibrahim OS v2.5\nC Derleyicisi Devrede!";
        fs_write(readme, msg, str_len(msg));
    }

    // Örnek 1: C Programı (Döngü ve Sayı Hesaplama)
    int c_app1 = fs_create(0, "hesap.exc", FS_TYPE_FILE, 20, 95);
    if (c_app1 != -1) {
        const char* c_code = 
            "int main() {\n"
            "    print(\"--- C Programi Basladi ---\");\n"
            "    int a = 15;\n"
            "    int b = 25;\n"
            "    int toplam = a + b;\n"
            "    print(\"Toplam Sonucu:\");\n"
            "    print_num(toplam);\n"
            "    beep(440, 100);\n"
            "    beep(880, 200);\n"
            "    print(\"Program bitti.\");\n"
            "    return 0;\n"
            "}\n";
        fs_write(c_app1, c_code, str_len(c_code));
    }

    // Örnek 2: Melodi Çalan C Programı
    int c_app2 = fs_create(0, "muzik.exc", FS_TYPE_FILE, 20, 170);
    if (c_app2 != -1) {
        const char* music_code = 
            "int main() {\n"
            "    print(\"Muzik caliniyor...\");\n"
            "    int n = 0;\n"
            "    while (n < 4) {\n"
            "        beep(523, 80);\n"
            "        beep(659, 80);\n"
            "        beep(784, 100);\n"
            "        n = n + 1;\n"
            "    }\n"
            "    print(\"Muzik bitti!\");\n"
            "    return 0;\n"
            "}\n";
        fs_write(c_app2, music_code, str_len(music_code));
    }

    int executable = fs_create(0, "hosgeldin.exc", FS_TYPE_FILE, 20, 320);
    if (executable != -1) {
        const char* executable_code =
            "int main() {\n"
            "    print(\"EXC programi calisti.\");\n"
            "    return 0;\n"
            "}\n";
        fs_write(executable, executable_code, str_len(executable_code));
    }

    int lb_app = fs_create(0, "panel.lb", FS_TYPE_FILE, 150, 320);
    if (lb_app != -1) {
        const char* lb_code =
            "BASLIK Ibrahim Panel\n"
            "ARKA MAVI\n"
            "YAZI 20 20 BEYAZ .lb uygulamasi\n"
            "BUTON 20 60 120 30 Ses BIP 0\n";
        fs_write(lb_app, lb_code, str_len(lb_code));
    }

    int sys_dir = fs_create(0, "Sistem", FS_TYPE_DIR, 20, 245);
    if (sys_dir != -1) {
        int log = fs_create(sys_dir, "kernel.log", FS_TYPE_FILE, 0, 0);
        if (log != -1) {
            const char* log_msg = "Kernel yuklendi.\nC Compiler & VM devrede.";
            fs_write(log, log_msg, str_len(log_msg));
        }
    }
}

int fs_create(int parent_id, const char* name, int type, int x, int y) {
    int free_slot = -1;
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs_table[i].is_used) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == -1) return -1;

    fs_table[free_slot].is_used = 1;
    fs_table[free_slot].type = type;
    fs_table[free_slot].parent_id = parent_id;
    fs_table[free_slot].size = 0;
    fs_table[free_slot].content[0] = '\0';
    fs_table[free_slot].x = x;
    fs_table[free_slot].y = y;
    fs_table[free_slot].kind = (type == FS_TYPE_FILE) ? fs_kind_for_name(name) : FS_KIND_TEXT;
    str_copy(fs_table[free_slot].name, name);

    return free_slot;
}

int fs_get_kind(int node_id) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) {
        return FS_KIND_TEXT;
    }
    if (fs_table[node_id].kind == FS_KIND_EXECUTABLE &&
        fs_table[node_id].size >= 7 && str_cmp(fs_table[node_id].content, "IBEXC0;") == 0) {
        return FS_KIND_EXECUTABLE_CLOSED;
    }
    return fs_table[node_id].kind;
}

int fs_is_runnable(int node_id) {
    int kind = fs_get_kind(node_id);
    return kind == FS_KIND_SOURCE || kind == FS_KIND_EXECUTABLE;
}

int fs_is_editable(int node_id) {
    int kind = fs_get_kind(node_id);
    return kind == FS_KIND_TEXT || kind == FS_KIND_SOURCE ||
        kind == FS_KIND_EXECUTABLE || kind == FS_KIND_LIBRARY;
}

int fs_move(int node_id, int parent_id) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) return 0;
    if (parent_id < 0 || parent_id >= FS_MAX_NODES ||
        (parent_id != 0 && (!fs_table[parent_id].is_used || fs_table[parent_id].type != FS_TYPE_DIR))) {
        return 0;
    }
    if (node_id == parent_id) return 0;

    int ancestor = parent_id;
    while (ancestor != 0) {
        if (ancestor == node_id) return 0;
        ancestor = fs_table[ancestor].parent_id;
    }

    fs_table[node_id].parent_id = parent_id;
    if (parent_id != 0) {
        fs_table[node_id].x = 0;
        fs_table[node_id].y = 0;
    }
    return 1;
}

int fs_rename(int node_id, const char* name) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used ||
        !name || str_len(name) == 0 || str_len(name) >= FS_NAME_MAX) return 0;
    if (fs_find_child(fs_table[node_id].parent_id, name) != -1 &&
        fs_find_child(fs_table[node_id].parent_id, name) != node_id) return 0;

    str_copy(fs_table[node_id].name, name);
    if (fs_table[node_id].type == FS_TYPE_FILE) {
        fs_table[node_id].kind = fs_kind_for_name(name);
    }
    return 1;
}

int fs_find_child(int parent_id, const char* name) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_table[i].is_used && fs_table[i].parent_id == parent_id && str_cmp(fs_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_delete(int node_id) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) return 0;

    if (fs_table[node_id].type == FS_TYPE_DIR) {
        for (int i = 0; i < FS_MAX_NODES; i++) {
            if (fs_table[i].is_used && fs_table[i].parent_id == node_id) {
                fs_delete(i);
            }
        }
    }

    fs_table[node_id].is_used = 0;
    fs_table[node_id].size = 0;
    fs_table[node_id].content[0] = '\0';
    fs_table[node_id].name[0] = '\0';
    return 1;
}

int fs_write(int node_id, const char* data, int len) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) return 0;
    if (len >= FS_FILE_MAX_SIZE) len = FS_FILE_MAX_SIZE - 1;

    for (int i = 0; i < len; i++) {
        fs_table[node_id].content[i] = data[i];
    }
    fs_table[node_id].content[len] = '\0';
    fs_table[node_id].size = len;
    return len;
}

int fs_insert_char(int node_id, int index, char c) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used ||
        fs_table[node_id].type != FS_TYPE_FILE || index < 0 || index > fs_table[node_id].size ||
        fs_table[node_id].size >= FS_FILE_MAX_SIZE - 1) return 0;
    for (int i = fs_table[node_id].size; i >= index; i--) {
        fs_table[node_id].content[i + 1] = fs_table[node_id].content[i];
    }
    fs_table[node_id].content[index] = c;
    fs_table[node_id].size++;
    return 1;
}

int fs_append_char(int node_id, char c) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) return 0;
    if (fs_table[node_id].size >= FS_FILE_MAX_SIZE - 1) return 0;

    int idx = fs_table[node_id].size;
    fs_table[node_id].content[idx] = c;
    fs_table[node_id].content[idx + 1] = '\0';
    fs_table[node_id].size++;
    return 1;
}

int fs_backspace(int node_id) {
    if (node_id < 0 || node_id >= FS_MAX_NODES || !fs_table[node_id].is_used) return 0;
    if (fs_table[node_id].size <= 0) return 0;

    fs_table[node_id].size--;
    fs_table[node_id].content[fs_table[node_id].size] = '\0';
    return 1;
}
