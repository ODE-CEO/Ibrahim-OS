#ifndef FS_H
#define FS_H

#include "os.h"

#define FS_MAX_NODES      64
#define FS_NAME_MAX       24
#define FS_FILE_MAX_SIZE  2048

#define FS_TYPE_FILE      0
#define FS_TYPE_DIR       1

enum {
    FS_KIND_TEXT = 0,
    FS_KIND_SOURCE,
    FS_KIND_LIBRARY,
    FS_KIND_EXECUTABLE,
    FS_KIND_EXECUTABLE_CLOSED,
    FS_KIND_OBJECT,
    FS_KIND_COUNT
};

typedef struct {
    int is_used;
    int type;
    int parent_id;
    char name[FS_NAME_MAX];
    char content[FS_FILE_MAX_SIZE];
    int size;
    int x, y;
    int kind;
} fs_node_t;

extern fs_node_t fs_table[FS_MAX_NODES];

void fs_init(void);
void fs_format(void);
int fs_create(int parent_id, const char* name, int type, int x, int y);
int fs_find_child(int parent_id, const char* name);
int fs_get_kind(int node_id);
int fs_is_runnable(int node_id);
int fs_is_editable(int node_id);
int fs_move(int node_id, int parent_id);
int fs_rename(int node_id, const char* name);
int fs_delete(int node_id);
int fs_write(int node_id, const char* data, int len);
int fs_insert_char(int node_id, int index, char c);
int fs_append_char(int node_id, char c);
int fs_backspace(int node_id);

#endif
