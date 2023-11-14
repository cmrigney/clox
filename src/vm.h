#ifndef clox_vm_h
#define clox_vm_h

#include <limits.h>

#include "object.h"
#include "table.h"
#include "value.h"

#ifdef PICO_MODULE
// Preserve memory as much as possible
#define MAX_NATIVE_MODULES 2
#define FRAMES_MAX 20
#define LOX_PATH_MAX 10
#else
#define MAX_NATIVE_MODULES 256
#define FRAMES_MAX 64
#define LOX_PATH_MAX PATH_MAX
#endif
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value* stackTop;
  Table globals;
  Table strings;
  ObjString* initString;
  ObjUpvalue* openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;
  size_t debug_maxTotalAllocated;
  Obj* objects;
  int grayCount;
  int grayCapacity;
  Obj** grayStack;

  void *nativeModules[MAX_NATIVE_MODULES];
  int nativeModuleCount;

  char workingDirectory[LOX_PATH_MAX];
  char scriptName[LOX_PATH_MAX];
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

EXPORT void initVM();
EXPORT void freeVM();
EXPORT InterpretResult interpret(const char* source);
EXPORT InterpretResult validate(const char* source);
EXPORT void push(Value value);
EXPORT Value pop();
EXPORT Value peek(int distance);
EXPORT Value callLoxCode(const char* name, Value *receiver, int argCount, Value *args);
EXPORT void mutateConcatenate();
EXPORT void concatenate();
EXPORT void closeUpvalues(Value* last);
EXPORT bool callModule(ObjClosure *closure, int argCount);
EXPORT ObjInstance *createObjectInstance();
EXPORT bool callValue(Value callee, int argCount);
EXPORT ObjUpvalue* captureUpvalue(Value* local);
EXPORT void defineMethod(ObjString* name);
EXPORT bool invoke(ObjString* name, int argCount);
EXPORT bool bindMethod(ObjClass* klass, ObjString* name);

#endif
