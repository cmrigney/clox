#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
ObjFunction* compileModule(const char* source);
ObjFunction* compileEval(const char* source);
void markCompilerRoots();

#endif
