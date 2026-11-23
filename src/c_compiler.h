#ifndef C_COMPILER_H
#define C_COMPILER_H

#include "os.h"

void c_engine_init(void);
int c_compile_and_run(const char* source_code);

#endif
