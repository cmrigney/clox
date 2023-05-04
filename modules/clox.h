#ifndef clox_header_h
#define clox_header_h

// Modules can be written in C++ but the core is C
#ifdef __cplusplus
extern "C" {
#endif
#include "../src/modules.h"
#include "../src/vm.h"
#include "../src/object.h"
#include "../src/value.h"
#include "../src/compiler.h"
#include "../src/memory.h"
#ifdef __cplusplus
}
#endif

#endif