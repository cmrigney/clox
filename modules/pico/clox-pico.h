#ifndef CLOX_PICO_MODULE_H
#define CLOX_PICO_MODULE_H

#include <stdbool.h>

bool registerModule_pico();
void pico_repl();
void unregisterModule_pico();
void pico_exit(int exit_code);

#endif
