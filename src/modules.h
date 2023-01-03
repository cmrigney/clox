#ifndef clox_modules_h
#define clox_modules_h

#include "value.h"
#include "object.h"
#include "vm.h"

typedef bool (*RegisterModule)();

Value systemImportNative(Value *receiver, int argCount, Value *args);

void freeNativeModules();

// Used by modules

void registerNativeMethod(const char *name, NativeFn function);

// Statically linked modules
#ifdef FILESYSTEM_MODULE
#include "filesystem/filesystem.h"
#endif
#ifdef OS_MODULE
#include "os/os.h"
#endif
#ifdef PICO_MODULE
#include "pico/pico.h"
#endif

#endif
