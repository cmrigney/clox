#ifndef clox_modules_h
#define clox_modules_h

#include "value.h"
#include "object.h"
#include "vm.h"

#ifndef WASM

typedef bool (*RegisterModule)();

Value systemImportNative(Value *receiver, int argCount, Value *args);

void freeNativeModules();

// Used by modules

void registerNativeMethod(const char *name, NativeFn function);

#endif

#endif