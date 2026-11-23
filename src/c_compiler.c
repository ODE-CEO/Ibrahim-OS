#include "c_compiler.h"
#include "speaker.h"
#include "heap.h"

extern void term_print(const char* text);

// Bytecode Sanal Makine Komutları
enum {
    OP_LEA, OP_IMM, OP_JMP, OP_JZ, OP_JNZ, OP_CALL,
    OP_NATIVE, OP_ENT, OP_ADJ, OP_LEV, OP_LI, OP_LC,
    OP_SI, OP_SC, OP_PSH, OP_OR, OP_XOR, OP_AND,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_SHL, OP_SHR, OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_MOD, OP_EXIT
};

// Token Tipleri
enum {
    NUM = 128, ID, CHAR_LIT, STR_LIT,
    KW_INT, KW_CHAR, KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    KW_PRINT, KW_PRINT_NUM, KW_BEEP, KW_SLEEP, KW_CLS,
    ASSIGN, OR_OP, AND_OP, EQ_OP, NE_OP, LE_OP, GE_OP, SHL_OP, SHR_OP
};

#define POOL_SIZE 32768

static int* text_segment;
static int* stack_segment;
static char* data_segment;

static int* pc;
static int* sp;
static int* bp;
static int ax, cycle;

static const char* src_ptr;
static int token;
static int token_val;
static char token_ident[64];

// Sembol Tablosu
typedef struct symbol {
    char name[32];
    int token_type;
    int value_type;
    int value;
    int class; // Global veya Local
} symbol_t;

#define MAX_SYMBOLS 256
static symbol_t sym_table[MAX_SYMBOLS];
static int sym_count = 0;
static int local_offset = 0;

static symbol_t* find_symbol(const char* name) {
    for (int i = sym_count - 1; i >= 0; i--) {
        if (str_cmp(sym_table[i].name, name) == 0) return &sym_table[i];
    }
    return 0;
}

static symbol_t* add_symbol(const char* name, int type, int cls) {
    if (sym_count >= MAX_SYMBOLS) return 0;
    symbol_t* s = &sym_table[sym_count++];
    str_copy(s->name, name);
    s->token_type = ID;
    s->value_type = type;
    s->class = cls;
    s->value = 0;
    return s;
}

static void next_token(void) {
    while (*src_ptr == ' ' || *src_ptr == '\t' || *src_ptr == '\r' || *src_ptr == '\n') {
        src_ptr++;
    }

    if (*src_ptr == '\0') {
        token = 0;
        return;
    }

    // Yorum Satırları (// ve /* */)
    if (*src_ptr == '/' && *(src_ptr + 1) == '/') {
        while (*src_ptr && *src_ptr != '\n') src_ptr++;
        next_token();
        return;
    }
    if (*src_ptr == '/' && *(src_ptr + 1) == '*') {
        src_ptr += 2;
        while (*src_ptr && !(*src_ptr == '*' && *(src_ptr + 1) == '/')) src_ptr++;
        if (*src_ptr) src_ptr += 2;
        next_token();
        return;
    }

    // Sayılar
    if (*src_ptr >= '0' && *src_ptr <= '9') {
        token_val = 0;
        while (*src_ptr >= '0' && *src_ptr <= '9') {
            token_val = token_val * 10 + (*src_ptr++ - '0');
        }
        token = NUM;
        return;
    }

    // Karakter Katarları (Strings)
    if (*src_ptr == '"') {
        src_ptr++;
        int d_idx = 0;
        char* str_start = (char*)data_segment + local_offset;
        while (*src_ptr && *src_ptr != '"') {
            if (*src_ptr == '\\' && *(src_ptr + 1) == 'n') {
                str_start[d_idx++] = '\n';
                src_ptr += 2;
            } else {
                str_start[d_idx++] = *src_ptr++;
            }
        }
        if (*src_ptr == '"') src_ptr++;
        str_start[d_idx++] = '\0';
        token_val = (int)str_start;
        local_offset += d_idx;
        token = STR_LIT;
        return;
    }

    // İdentifier ve Anahtar Kelimeler
    if ((*src_ptr >= 'a' && *src_ptr <= 'z') || (*src_ptr >= 'A' && *src_ptr <= 'Z') || *src_ptr == '_') {
        int idx = 0;
        while (((*src_ptr >= 'a' && *src_ptr <= 'z') || (*src_ptr >= 'A' && *src_ptr <= 'Z') || (*src_ptr >= '0' && *src_ptr <= '9') || *src_ptr == '_') && idx < 63) {
            token_ident[idx++] = *src_ptr++;
        }
        token_ident[idx] = '\0';

        if (str_cmp(token_ident, "int") == 0) token = KW_INT;
        else if (str_cmp(token_ident, "char") == 0) token = KW_CHAR;
        else if (str_cmp(token_ident, "if") == 0) token = KW_IF;
        else if (str_cmp(token_ident, "else") == 0) token = KW_ELSE;
        else if (str_cmp(token_ident, "while") == 0) token = KW_WHILE;
        else if (str_cmp(token_ident, "for") == 0) token = KW_FOR;
        else if (str_cmp(token_ident, "return") == 0) token = KW_RETURN;
        else if (str_cmp(token_ident, "print") == 0) token = KW_PRINT;
        else if (str_cmp(token_ident, "print_num") == 0) token = KW_PRINT_NUM;
        else if (str_cmp(token_ident, "beep") == 0) token = KW_BEEP;
        else if (str_cmp(token_ident, "sleep") == 0) token = KW_SLEEP;
        else if (str_cmp(token_ident, "cls") == 0) token = KW_CLS;
        else token = ID;
        return;
    }

    // Operatörler
    char c = *src_ptr++;
    if (c == '=' && *src_ptr == '=') { src_ptr++; token = EQ_OP; }
    else if (c == '!' && *src_ptr == '=') { src_ptr++; token = NE_OP; }
    else if (c == '<' && *src_ptr == '=') { src_ptr++; token = LE_OP; }
    else if (c == '>' && *src_ptr == '=') { src_ptr++; token = GE_OP; }
    else if (c == '&' && *src_ptr == '&') { src_ptr++; token = AND_OP; }
    else if (c == '|' && *src_ptr == '|') { src_ptr++; token = OR_OP; }
    else token = c;
}

static void match(int expected) {
    if (token == expected) next_token();
}

static int* code_ptr;
static void emit(int op) {
    *code_ptr++ = op;
}

// Öncelik Sırasına Göre İfade (Expression) Ayrıştırıcı
static void parse_expr(int level);

static void parse_factor(void) {
    if (token == NUM) {
        emit(OP_IMM);
        emit(token_val);
        next_token();
    }
    else if (token == STR_LIT) {
        emit(OP_IMM);
        emit(token_val);
        next_token();
    }
    else if (token == ID) {
        symbol_t* s = find_symbol(token_ident);
        if (!s) s = add_symbol(token_ident, KW_INT, 0); // Bulunamazsa otomatik ekle
        next_token();

        if (token == '(') { // Fonksiyon Çağrısı
            next_token();
            int args = 0;
            while (token != ')') {
                parse_expr(ASSIGN);
                emit(OP_PSH);
                args++;
                if (token == ',') next_token();
            }
            match(')');
            emit(OP_CALL);
            emit(s->value);
            if (args > 0) {
                emit(OP_ADJ);
                emit(args);
            }
        } else { // Değişken Okuma
            emit(OP_LEA);
            emit(s->value);
            emit(OP_LI);
        }
    }
    else if (token == '(') {
        next_token();
        parse_expr(ASSIGN);
        match(')');
    }
    else if (token == KW_PRINT || token == KW_PRINT_NUM || token == KW_BEEP || token == KW_SLEEP || token == KW_CLS) {
        int syscall_type = token;
        next_token();
        match('(');
        int args = 0;
        while (token != ')') {
            parse_expr(ASSIGN);
            emit(OP_PSH);
            args++;
            if (token == ',') next_token();
        }
        match(')');
        emit(OP_NATIVE);
        emit(syscall_type);
        if (args > 0) {
            emit(OP_ADJ);
            emit(args);
        }
    }
}

static void parse_expr(int level) {
    if (level == ASSIGN) {
        if (token == ID) {
            symbol_t* s = find_symbol(token_ident);
            if (!s) s = add_symbol(token_ident, KW_INT, 0);
            const char* saved_src = src_ptr;
            int saved_tok = token;
            char saved_ident[64];
            str_copy(saved_ident, token_ident);
            next_token();

            if (token == '=') {
                next_token();
                parse_expr(ASSIGN);
                emit(OP_PSH);
                emit(OP_LEA);
                emit(s->value);
                emit(OP_SI);
                return;
            } else {
                src_ptr = saved_src;
                token = saved_tok;
                str_copy(token_ident, saved_ident);
            }
        }
    }

    parse_factor();

    while (token == '+' || token == '-' || token == '*' || token == '/' || token == '%' ||
           token == EQ_OP || token == NE_OP || token == '<' || token == '>' || token == LE_OP || token == GE_OP) {
        int op = token;
        next_token();
        emit(OP_PSH);
        parse_factor();
        switch (op) {
            case '+': emit(OP_ADD); break;
            case '-': emit(OP_SUB); break;
            case '*': emit(OP_MUL); break;
            case '/': emit(OP_DIV); break;
            case '%': emit(OP_MOD); break;
            case EQ_OP: emit(OP_EQ); break;
            case NE_OP: emit(OP_NE); break;
            case '<': emit(OP_LT); break;
            case '>': emit(OP_GT); break;
            case LE_OP: emit(OP_LE); break;
            case GE_OP: emit(OP_GE); break;
        }
    }
}

static void parse_statement(void);

static void parse_statement(void) {
    if (token == KW_IF) {
        next_token();
        match('(');
        parse_expr(ASSIGN);
        match(')');
        emit(OP_JZ);
        int* jz_patch = code_ptr++;
        parse_statement();
        if (token == KW_ELSE) {
            next_token();
            emit(OP_JMP);
            int* jmp_patch = code_ptr++;
            *jz_patch = (int)(code_ptr - text_segment);
            parse_statement();
            *jmp_patch = (int)(code_ptr - text_segment);
        } else {
            *jz_patch = (int)(code_ptr - text_segment);
        }
    }
    else if (token == KW_WHILE) {
        next_token();
        int loop_start = (int)(code_ptr - text_segment);
        match('(');
        parse_expr(ASSIGN);
        match(')');
        emit(OP_JZ);
        int* jz_patch = code_ptr++;
        parse_statement();
        emit(OP_JMP);
        emit(loop_start);
        *jz_patch = (int)(code_ptr - text_segment);
    }
    else if (token == KW_RETURN) {
        next_token();
        if (token != ';') parse_expr(ASSIGN);
        match(';');
        emit(OP_LEV);
    }
    else if (token == '{') {
        next_token();
        while (token && token != '}') parse_statement();
        match('}');
    }
    else if (token == KW_INT || token == KW_CHAR) {
        next_token();
        while (token == ID) {
            add_symbol(token_ident, KW_INT, 0);
            symbol_t* s = find_symbol(token_ident);
            s->value = (int)(stack_segment + sym_count);
            next_token();
            if (token == '=') {
                next_token();
                parse_expr(ASSIGN);
                emit(OP_PSH);
                emit(OP_LEA);
                emit(s->value);
                emit(OP_SI);
            }
            if (token == ',') next_token();
        }
        match(';');
    }
    else {
        parse_expr(ASSIGN);
        match(';');
    }
}

void c_engine_init(void) {
    heap_init();
    text_segment  = (int*)kmalloc(POOL_SIZE * sizeof(int));
    stack_segment = (int*)kmalloc(POOL_SIZE * sizeof(int));
    data_segment  = (char*)kmalloc(POOL_SIZE);
}

int c_compile_and_run(const char* source_code) {
    if (!text_segment) c_engine_init();

    code_ptr = text_segment;
    src_ptr = source_code;
    sym_count = 0;
    local_offset = 0;

    next_token();

    // Giriş Fonksiyonu Kontrolü
    int main_entry = 0;

    while (token) {
        if (token == KW_INT || token == KW_CHAR) next_token();
        if (token == ID) {
            char func_name[64];
            str_copy(func_name, token_ident);
            next_token();
            if (token == '(') {
                match('(');
                while (token != ')') next_token();
                match(')');
                symbol_t* fn = add_symbol(func_name, KW_INT, 1);
                fn->value = (int)(code_ptr - text_segment);
                if (str_cmp(func_name, "main") == 0) {
                    main_entry = fn->value;
                }
                emit(OP_ENT);
                emit(0);
                parse_statement();
                emit(OP_LEV);
            }
        } else {
            parse_statement();
        }
    }

    int exit_entry = (int)(code_ptr - text_segment);
    emit(OP_EXIT);

    // Bytecode Sanal Makineyi Başlat
    sp = stack_segment + POOL_SIZE - 1;
    bp = sp;
    *sp = (int)(text_segment + exit_entry);
    pc = text_segment + main_entry;
    ax = 0;
    cycle = 0;

    while (1) {
        cycle++;
        int op = *pc++;

        if (op == OP_IMM) { ax = *pc++; }
        else if (op == OP_LEA) { ax = *pc++; }
        else if (op == OP_JMP) { pc = text_segment + *pc; }
        else if (op == OP_JZ)  { pc = (ax == 0) ? (text_segment + *pc) : (pc + 1); }
        else if (op == OP_JNZ) { pc = (ax != 0) ? (text_segment + *pc) : (pc + 1); }
        else if (op == OP_CALL) {
            *--sp = (int)(pc + 1);
            pc = text_segment + *pc;
        }
        else if (op == OP_ENT) {
            *--sp = (int)bp;
            bp = sp;
            sp -= *pc++;
        }
        else if (op == OP_ADJ) { sp += *pc++; }
        else if (op == OP_LEV) {
            sp = bp;
            bp = (int*)*sp++;
            pc = (int*)*sp++;
        }
        else if (op == OP_LI)  { ax = *(int*)ax; }
        else if (op == OP_LC)  { ax = *(char*)ax; }
        else if (op == OP_SI)  { *(int*)*sp++ = ax; }
        else if (op == OP_SC)  { *(char*)*sp++ = (char)ax; }
        else if (op == OP_PSH) { *--sp = ax; }
        else if (op == OP_ADD) { ax = *sp++ + ax; }
        else if (op == OP_SUB) { ax = *sp++ - ax; }
        else if (op == OP_MUL) { ax = *sp++ * ax; }
        else if (op == OP_DIV) { ax = (ax != 0) ? (*sp++ / ax) : 0; }
        else if (op == OP_MOD) { ax = (ax != 0) ? (*sp++ % ax) : 0; }
        else if (op == OP_EQ)  { ax = (*sp++ == ax); }
        else if (op == OP_NE)  { ax = (*sp++ != ax); }
        else if (op == OP_LT)  { ax = (*sp++ < ax); }
        else if (op == OP_GT)  { ax = (*sp++ > ax); }
        else if (op == OP_LE)  { ax = (*sp++ <= ax); }
        else if (op == OP_GE)  { ax = (*sp++ >= ax); }
        else if (op == OP_NATIVE) {
            int sys = *pc++;
            if (sys == KW_PRINT) {
                char* str = (char*)*sp;
                term_print(str);
            }
            else if (sys == KW_PRINT_NUM) {
                int num = *sp;
                char buf[20];
                itoa(num, buf);
                term_print(buf);
            }
            else if (sys == KW_BEEP) {
                int freq = *sp;
                int ms = *(sp + 1);
                speaker_beep(freq, ms);
            }
            else if (sys == KW_SLEEP) {
                int ms = *sp;
                sleep_ms(ms);
            }
            else if (sys == KW_CLS) {
                extern void term_clear(void);
                term_clear();
            }
        }
        else if (op == OP_EXIT) {
            break;
        }
    }

    return ax;
}
