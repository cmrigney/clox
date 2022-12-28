#ifndef clox_modules_h
#define clox_modules_h

#include "value.h"
#include "object.h"

typedef bool (*RegisterModule)();

Value nativeImportNative(Value *receiver, int argCount, Value *args);

void freeNativeModules();

// Used by modules

void registerNativeMethod(const char *name, NativeFn function);

#endif