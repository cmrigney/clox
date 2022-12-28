#ifndef WASM

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "vm.h"
#include "modules.h"

extern VM vm;

Value systemImportNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("systemImport() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("systemImport() argument must be a string.");
    return NIL_VAL;
  }
  char *path = malloc(strlen(AS_CSTRING(args[0])) * 2 + 30);
  sprintf(path, "modules/%s/libclox%s.so", AS_CSTRING(args[0]), AS_CSTRING(args[0]));
  void *module = dlopen(path, RTLD_NOW);
  free(path);
  if(module == NULL) {
    printf("Can't find module: %s\n", dlerror());
    // runtimeError("systemImport() failed to load module: %s", dlerror());
    return NIL_VAL;
  }
  vm.nativeModules[vm.nativeModuleCount++] = module;

  // TODO dedupe imports
  char moduleName[256];
  sprintf(moduleName, "__Module%d_%s", vm.nativeModuleCount - 1, AS_CSTRING(args[0]));
  push(OBJ_VAL(copyString(moduleName, strlen(moduleName))));
  ObjClass *moduleClass = newClass(AS_STRING(peek(0)));
  push(OBJ_VAL(moduleClass));
  // Add methods to moduleClass
  ObjInstance *moduleInstance = newInstance(moduleClass);
  push(OBJ_VAL(moduleInstance));
  RegisterModule registerFn = dlsym(module, "registerModule");
  if(registerFn == NULL) {
    printf("Can't find register fn\n");
    // runtimeError("systemImport() failed to load registerModule() function: %s", dlerror());
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
  for (int i = 0; i < vm.nativeModuleCount; i++) {
    dlclose(vm.nativeModules[i]);
  }
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

#endif