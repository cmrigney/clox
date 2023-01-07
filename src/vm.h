#ifndef clox_vm_h
#define clox_vm_h

#include <limits.h>

#include "object.h"
#include "table.h"
#include "value.h"

#define MAX_NATIVE_MODULES 256
#define FRAMES_MAX 64
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
  Obj* objects;
  int grayCount;
  int grayCapacity;
  Obj** grayStack;

  void *nativeModules[MAX_NATIVE_MODULES];
  int nativeModuleCount;

  char workingDirectory[PATH_MAX];
  char scriptName[PATH_MAX];
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
Value peek(int distance);
Value callLoxCode(const char* name, Value *receiver, int argCount, Value *args);
void concatenate();
bool callModule(ObjClosure *closure, int argCount);

#endif
