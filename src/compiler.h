#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

EXPORT ObjFunction* compile(const char* source);
EXPORT ObjFunction* compileModule(const char* source);
EXPORT void markCompilerRoots();

#endif
