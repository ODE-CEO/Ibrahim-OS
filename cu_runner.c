#include "cu_runner.h"
#include "speaker.h"

cu_app_context_t current_cu_app;

static uint32_t parse_color(const char* str) {
    if (str_cmp(str, "SIYAH") == 0 || str_cmp(str, "BLACK") == 0) return 0x000000;
    if (str_cmp(str, "BEYAZ") == 0 || str_cmp(str, "WHITE") == 0) return 0xFFFFFF;
    if (str_cmp(str, "KIRMIZI") == 0 || str_cmp(str, "RED") == 0) return 0xE74C3C;
    if (str_cmp(str, "YESIL") == 0 || str_cmp(str, "GREEN") == 0) return 0x2ECC71;
    if (str_cmp(str, "MAVI") == 0 || str_cmp(str, "BLUE") == 0) return 0x3498DB;
    if (str_cmp(str, "SARI") == 0 || str_cmp(str, "YELLOW") == 0) return 0xF1C40F;
    if (str_cmp(str, "GRI") == 0 || str_cmp(str, "GRAY") == 0) return 0x95A5A6;
    if (str_cmp(str, "TURUNCU") == 0 || str_cmp(str, "ORANGE") == 0) return 0xE67E22;
    if (str_cmp(str, "KOYU") == 0 || str_cmp(str, "DARK") == 0) return 0x2C3E50;
    
    // Hex parsing (0xRRGGBB)
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        uint32_t val = 0;
        int i = 2;
        while (str[i]) {
            char c = str[i++];
            uint32_t digit = 0;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else break;
            val = (val << 4) | digit;
        }
        return val;
    }
    return 0x000000;
}

static int parse_int(const char* str) {
    int res = 0;
    int i = 0;
    int sign = 1;
    if (str[0] == '-') { sign = -1; i++; }
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return res * sign;
}

static int get_token(const char* src, int* pos, char* dest, int max_len) {
    while (src[*pos] == ' ' || src[*pos] == '\t' || src[*pos] == '\r') (*pos)++;
    if (src[*pos] == '\0' || src[*pos] == '\n') return 0;

    int d = 0;
    while (src[*pos] && src[*pos] != ' ' && src[*pos] != '\t' && src[*pos] != '\n' && src[*pos] != '\r' && d < max_len - 1) {
        dest[d++] = src[(*pos)++];
    }
    dest[d] = '\0';
    return 1;
}

void cu_init(void) {
    str_copy(current_cu_app.title, "CU Uygulamasi");
    current_cu_app.bg_color = COLOR_WHITE;
    current_cu_app.button_count = 0;
    current_cu_app.raw_code[0] = '\0';
}

void cu_run_script(const char* script) {
    cu_init();
    str_copy(current_cu_app.raw_code, script);

    int pos = 0;
    char tok[32];

    while (script[pos]) {
        if (!get_token(script, &pos, tok, 32)) {
            if (script[pos] == '\n') pos++;
            else if (script[pos] == '\0') break;
            continue;
        }

        if (str_cmp(tok, "BASLIK") == 0 || str_cmp(tok, "TITLE") == 0) {
            get_token(script, &pos, current_cu_app.title, 32);
        }
        else if (str_cmp(tok, "ARKA") == 0 || str_cmp(tok, "BG") == 0) {
            char color_tok[20];
            get_token(script, &pos, color_tok, 20);
            current_cu_app.bg_color = parse_color(color_tok);
        }
        else if (str_cmp(tok, "SES") == 0 || str_cmp(tok, "BEEP") == 0) {
            char f_tok[10], d_tok[10];
            get_token(script, &pos, f_tok, 10);
            get_token(script, &pos, d_tok, 10);
            speaker_beep(parse_int(f_tok), parse_int(d_tok));
        }
        else if (str_cmp(tok, "BUTON") == 0 || str_cmp(tok, "BTN") == 0) {
            if (current_cu_app.button_count < MAX_CU_BUTTONS) {
                char x_s[10], y_s[10], w_s[10], h_s[10];
                int bi = current_cu_app.button_count;
                get_token(script, &pos, x_s, 10);
                get_token(script, &pos, y_s, 10);
                get_token(script, &pos, w_s, 10);
                get_token(script, &pos, h_s, 10);
                get_token(script, &pos, current_cu_app.buttons[bi].text, 20);
                get_token(script, &pos, current_cu_app.buttons[bi].action, 10);
                get_token(script, &pos, current_cu_app.buttons[bi].arg, 30);

                current_cu_app.buttons[bi].x = parse_int(x_s);
                current_cu_app.buttons[bi].y = parse_int(y_s);
                current_cu_app.buttons[bi].w = parse_int(w_s);
                current_cu_app.buttons[bi].h = parse_int(h_s);
                current_cu_app.buttons[bi].active = 1;
                current_cu_app.button_count++;
            }
        }

        // Satır sonuna kadar ilerle
        while (script[pos] && script[pos] != '\n') pos++;
        if (script[pos] == '\n') pos++;
    }
}

void cu_draw_app(int wx, int wy, int ww, int wh) {
    // Çalışma alanı arka planı
    draw_rect(wx + 5, wy + 30, ww - 10, wh - 35, current_cu_app.bg_color);

    const char* script = current_cu_app.raw_code;
    int pos = 0;
    char tok[32];

    while (script[pos]) {
        if (!get_token(script, &pos, tok, 32)) {
            if (script[pos] == '\n') pos++;
            else if (script[pos] == '\0') break;
            continue;
        }

        if (str_cmp(tok, "KUTU") == 0 || str_cmp(tok, "BOX") == 0) {
            char xs[10], ys[10], ws[10], hs[10], cs[20];
            get_token(script, &pos, xs, 10);
            get_token(script, &pos, ys, 10);
            get_token(script, &pos, ws, 10);
            get_token(script, &pos, hs, 10);
            get_token(script, &pos, cs, 20);
            draw_rect(wx + 10 + parse_int(xs), wy + 35 + parse_int(ys), parse_int(ws), parse_int(hs), parse_color(cs));
        }
        else if (str_cmp(tok, "YAZI") == 0 || str_cmp(tok, "TEXT") == 0) {
            char xs[10], ys[10], cs[20], txt[60];
            get_token(script, &pos, xs, 10);
            get_token(script, &pos, ys, 10);
            get_token(script, &pos, cs, 20);
            
            // Boşluklu yazıyı satır sonuna kadar al
            while (script[pos] == ' ' || script[pos] == '\t') pos++;
            int ti = 0;
            while (script[pos] && script[pos] != '\n' && script[pos] != '\r' && ti < 59) {
                txt[ti++] = script[pos++];
            }
            txt[ti] = '\0';
            draw_string(txt, wx + 10 + parse_int(xs), wy + 35 + parse_int(ys), parse_color(cs));
        }

        while (script[pos] && script[pos] != '\n') pos++;
        if (script[pos] == '\n') pos++;
    }

    // Butonları çiz
    for (int i = 0; i < current_cu_app.button_count; i++) {
        if (!current_cu_app.buttons[i].active) continue;
        int bx = wx + 10 + current_cu_app.buttons[i].x;
        int by = wy + 35 + current_cu_app.buttons[i].y;
        int bw = current_cu_app.buttons[i].w;
        int bh = current_cu_app.buttons[i].h;

        draw_rect(bx, by, bw, bh, 0x34495E);
        draw_rect(bx, by, bw, 2, 0x5D6D7E);
        draw_string(current_cu_app.buttons[i].text, bx + 8, by + (bh / 2) - 6, COLOR_WHITE);
    }
}

void cu_handle_click(int wx, int wy, int mx, int my) {
    for (int i = 0; i < current_cu_app.button_count; i++) {
        if (!current_cu_app.buttons[i].active) continue;
        int bx = wx + 10 + current_cu_app.buttons[i].x;
        int by = wy + 35 + current_cu_app.buttons[i].y;
        int bw = current_cu_app.buttons[i].w;
        int bh = current_cu_app.buttons[i].h;

        if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
            sound_click();
            if (str_cmp(current_cu_app.buttons[i].action, "SES") == 0) {
                speaker_beep(parse_int(current_cu_app.buttons[i].arg), 120);
            }
            else if (str_cmp(current_cu_app.buttons[i].action, "BIP") == 0) {
                speaker_beep(NOTE_A5, 80);
            }
            return;
        }
    }
}
