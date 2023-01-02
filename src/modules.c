#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef WASM
#include <dlfcn.h>
#endif

#include "common.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "vm.h"
#include "modules.h"

typedef struct {
  const char *name;
  RegisterModule registerFn;
} LinkedModule;

static LinkedModule linkedModules[] = {
  #ifdef FILESYSTEM_MODULE
  {"filesystem", registerModule_filesystem},
  #endif
  {NULL, NULL}
};

RegisterModule getStaticRegisterFunction(const char *name) {
  for(int i = 0; linkedModules[i].name != NULL; i++) {
    if(strcmp(linkedModules[i].name, name) == 0) {
      return linkedModules[i].registerFn;
    }
  }
  return NULL;
}

#ifndef WASM

RegisterModule getDynamicallyLoadedRegisterFunction(const char *name) {
  char *path = malloc(strlen(name) * 2 + 30);
  sprintf(path, "modules/%s/libclox%s.so", name, name);
  void *module = dlopen(path, RTLD_NOW);
  free(path);
  if(module == NULL) {
    printf("Can't find module: %s\n", dlerror());
    // runtimeError("systemImport() failed to load module: %s", dlerror());
    return NULL;
  }
  vm.nativeModules[vm.nativeModuleCount++] = module;

  // TODO dedupe imports
  char buffer[256];
  sprintf(buffer, "registerModule_%s", name);
  RegisterModule registerFn = dlsym(module, buffer);
  if(registerFn == NULL) {
    // runtimeError("systemImport() failed to load registerModule() function: %s", dlerror());
  }
  return registerFn;
}

#endif

static int moduleCount = 0;

Value systemImportNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("systemImport() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("systemImport() argument must be a string.");
    return NIL_VAL;
  }

  // TODO dedupe imports
  // Make an instance, put it at the top of the stack for registerNativeMethod
  char buffer[256];
  sprintf(buffer, "__Module%d_%s", moduleCount++, AS_CSTRING(args[0]));
  push(OBJ_VAL(copyString(buffer, strlen(buffer))));
  ObjClass *moduleClass = newClass(AS_STRING(peek(0)));
  push(OBJ_VAL(moduleClass));
  ObjInstance *moduleInstance = newInstance(moduleClass);
  push(OBJ_VAL(moduleInstance));

  RegisterModule registerFn = getStaticRegisterFunction(AS_CSTRING(args[0]));
  #ifndef WASM
  if(registerFn == NULL) {
    registerFn = getDynamicallyLoadedRegisterFunction(AS_CSTRING(args[0]));
    printf("Found dynamic module for %s\n", AS_CSTRING(args[0]));
  } else {
    printf("Found static module for %s\n", AS_CSTRING(args[0]));
  }
  #endif

  if(registerFn == NULL) {
    printf("Can't find register fn\n");
    pop();
    pop();
    pop();
    return NIL_VAL;
  }
  bool success = registerFn();
  pop();
  pop();
  pop();
  if(!success) {
    printf("Not successful\n");
    // runtimeError("systemImport() failed to register module.");
    return NIL_VAL;
  }
  return OBJ_VAL(moduleInstance);
}

void freeNativeModules() {
  #ifndef WASM
  for (int i = 0; i < vm.nativeModuleCount; i++) {
    dlclose(vm.nativeModules[i]);
  }
  #endif
  vm.nativeModuleCount = 0;
}

void registerNativeMethod(const char *name, NativeFn function) {
  if(!IS_INSTANCE(peek(0))) {
    // runtimeError("registerNativeMethod() called outside of register.");
    return;
  }
  ObjInstance *instance = AS_INSTANCE(peek(0));
  ObjString *methodName = copyString(name, strlen(name));
  push(OBJ_VAL(methodName));
  push(OBJ_VAL(newNative(function, false)));
  tableSet(&instance->klass->methods, methodName, peek(0));
  pop();
  pop();
}
