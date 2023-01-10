#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "native/native.h"
#include "modules.h"

VM vm;
static bool call(ObjClosure* closure, int argCount);
static bool callValue(Value callee, int argCount);

Value callLoxCode(const char* name, Value *receiver, int argCount, Value *args) {
  ObjString *key = copyString(name, (int)strlen(name));
  Value fn;
  if(!tableGet(&vm.globals, key, &fn)) {
    // runtimeError("Could not find function '%s'.", fnName);
    return NIL_VAL;
  }
  if (!IS_CLOSURE(fn)) {
    // runtimeError("Function '%s' is not a closure.", fnName);
    return NIL_VAL;
  }

  // Shift the args over one to make room for the receiver
  Value newArgs[argCount + 1];
  newArgs[0] = *receiver;
  for (int i = 0; i < argCount; i++) {
    newArgs[i + 1] = args[i];
  }

  for (int i = 0; i < argCount; i++) {
    pop(); // pop the args off
  }
  pop(); // pop the existing function off

  // Push the new function and args on
  push(fn);
  for (int i = 0; i < argCount + 1; i++) {
    push(newArgs[i]);
  }
  callValue(fn, argCount + 1);
  return NIL_VAL;
}

static ObjString* getBoundNativeFnName(ObjType type, const char* name) {
  char typeString[8];
  sprintf(typeString, "%d.", type);
  push(OBJ_VAL(copyString(typeString, (int)strlen(typeString))));
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  concatenate();
  return AS_STRING(pop());
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", 
            function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static void defineNative(const char* name, NativeFn function, bool callsLox) {
  // garbage collection care
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, callsLox)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

static void defineBoundNativeMethod(ObjType type, const char* name, NativeFn function, bool callsLox) {
  ObjString *key = getBoundNativeFnName(type, name);
  push(OBJ_VAL(key));
  push(OBJ_VAL(newNative(function, callsLox)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;

  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  vm.nativeModuleCount = 0;

  vm.scriptName[0] = '\0';
  #if defined WASM || defined PICO_MODULE
  vm.workingDirectory[0] = '/';
  #else
  if(getcwd(vm.workingDirectory, sizeof(vm.workingDirectory)) == NULL) {
    fprintf(stderr, "Could not get working directory.");
    exit(78);
  }
  #endif

  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.initString = NULL;
  vm.initString = copyString("init", 4);

  defineNative("clock", clockNative, false);
  defineNative("randN", randNNative, false);
  defineNative("parse", parseJsonNative, false);
  defineNative("stringify", stringifyJsonNative, false);
  defineNative("scanToEOF", scanToEOF, false);
  defineNative("log", printNative, false);
  defineNative("logln", printlnNative, false);
  defineNative("printMethods", printMethods, false);
  defineNative("getInstanceFields", getInstanceFields, false);
  defineNative("getInstanceFieldValueByKey", getInstanceFieldValueByKey, false);
  defineNative("setInstanceFieldValueByKey", setInstanceFieldValueByKey, false);
  defineNative("getEnvVar", getEnvVarNative, false);

  defineNative("systemImport", systemImportNative, true);

  defineNative("Array", array, false);
  defineBoundNativeMethod(OBJ_ARRAY, "count", array_count, false);
  defineBoundNativeMethod(OBJ_ARRAY, "push", array_push, false);
  defineBoundNativeMethod(OBJ_ARRAY, "get", array_get, false);
  defineBoundNativeMethod(OBJ_ARRAY, "pop", array_pop, false);
  defineBoundNativeMethod(OBJ_ARRAY, "filter", array_filter, true);
  defineBoundNativeMethod(OBJ_ARRAY, "map", array_map, true);
  defineBoundNativeMethod(OBJ_ARRAY, "forEach", array_foreach, true);

  defineNative("Buffer", bufferConstructor, false);
  defineBoundNativeMethod(OBJ_BUFFER, "length", buffer_length, false);
  defineBoundNativeMethod(OBJ_BUFFER, "get", buffer_get, false);
  defineBoundNativeMethod(OBJ_BUFFER, "set", buffer_set, false);
  defineBoundNativeMethod(OBJ_BUFFER, "asArray", buffer_as_array, false);
  defineBoundNativeMethod(OBJ_BUFFER, "asString", buffer_as_string, false);
  defineBoundNativeMethod(OBJ_BUFFER, "append", buffer_append, false);

  defineBoundNativeMethod(OBJ_STRING, "length", string_length, false);
  defineBoundNativeMethod(OBJ_STRING, "get", string_get, false);
  defineBoundNativeMethod(OBJ_STRING, "find", string_find, false);
  defineBoundNativeMethod(OBJ_STRING, "substring", string_substring, false);
  defineBoundNativeMethod(OBJ_STRING, "split", string_split, false);
  defineBoundNativeMethod(OBJ_STRING, "replace", string_replace, true);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();

  freeNativeModules();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

bool callModule(ObjClosure *closure, int argCount) {
  return call(closure, argCount);
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
        Value initializer;
        if (tableGet(&klass->methods, vm.initString,
                     &initializer)) {
          return call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d.",
                       argCount);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(NULL, argCount, vm.stackTop - argCount);
        if(!((ObjNative*)AS_OBJ(callee))->callsLox) {
          vm.stackTop -= argCount + 1;
          push(result);
        }
        return true;
      }
      case OBJ_BOUND_NATIVE: {
        ObjBoundNative *native = AS_BOUND_NATIVE(callee);
        Value result = native->function(&native->receiver, argCount, vm.stackTop - argCount);
        if(!((ObjBoundNative*)AS_OBJ(callee))->callsLox) {
          vm.stackTop -= argCount + 1;
          push(result);
        }
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  if(IS_CLOSURE(method)) {
    return call(AS_CLOSURE(method), argCount);
  }
  return callValue(method, argCount);
}

static bool invoke(ObjString* name, int argCount) {
  Value receiver = peek(argCount);

  // Handle native methods
  if(!IS_INSTANCE(receiver) && IS_OBJ(receiver)) {
    Value value;
    ObjString *key = getBoundNativeFnName(AS_OBJ(receiver)->type, name->chars);
    if(tableGet(&vm.globals, key, &value)) {
      Value bound = OBJ_VAL(newBoundNative(receiver, AS_NATIVE(value), ((ObjNative*)AS_OBJ(value))->callsLox));
      vm.stackTop[-argCount - 1] = bound;
      return callValue(bound, argCount);
    }
  }

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(receiver);

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(0),
                                         AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static bool bindNativeFn(Obj* obj, ObjString* name) {
  Value function;
  ObjString *key = getBoundNativeFnName(obj->type, name->chars);
  if(!tableGet(&vm.globals, key, &function)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  ObjBoundNative* bound = newBoundNative(peek(0), AS_NATIVE(function), ((ObjNative*)AS_OBJ(function))->callsLox);
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void concatenate() {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH() goto DO_DEBUG_PRINT
#else
#define DISPATCH() goto *dispatch_table[*frame->ip++]
#endif

  static void* dispatch_table[] = {
    &&DO_OP_NOT,
    &&DO_OP_NEGATE,
    &&DO_OP_PRINT,
    &&DO_OP_LOOP,
    &&DO_OP_JUMP,
    &&DO_OP_JUMP_IF_FALSE,
    &&DO_OP_CALL,
    &&DO_OP_INVOKE,
    &&DO_OP_SUPER_INVOKE,
    &&DO_OP_CLOSURE,
    &&DO_OP_CLOSE_UPVALUE,
    &&DO_OP_RETURN,
    &&DO_OP_CONSTANT,
    &&DO_OP_NIL,
    &&DO_OP_TRUE,
    &&DO_OP_FALSE,
    &&DO_OP_POP,
    &&DO_OP_GET_LOCAL,
    &&DO_OP_SET_LOCAL,
    &&DO_OP_GET_GLOBAL,
    &&DO_OP_DEFINE_GLOBAL,
    &&DO_OP_SET_GLOBAL,
    &&DO_OP_GET_UPVALUE,
    &&DO_OP_SET_UPVALUE,
    &&DO_OP_GET_PROPERTY,
    &&DO_OP_SET_PROPERTY,
    &&DO_OP_SET_PROPERTY_SHADOWED,
    &&DO_OP_GET_SUPER,
    &&DO_OP_EQUAL,
    &&DO_OP_GREATER,
    &&DO_OP_LESS,
    &&DO_OP_ADD,
    &&DO_OP_SUBTRACT,
    &&DO_OP_MULTIPLY,
    &&DO_OP_DIVIDE,
    &&DO_OP_CLASS,
    &&DO_OP_INHERIT,
    &&DO_OP_METHOD,
  };

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)
  
  DISPATCH();

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    DO_DEBUG_PRINT: {
      printf("          ");
      for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
      }
      printf("\n");
      disassembleInstruction(&frame->closure->function->chunk,
          (int)(frame->ip - frame->closure->function->chunk.code));
      goto *dispatch_table[*frame->ip++];
    }
    #endif

    DO_OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      // printValue(constant);
      // printf("\n");
      push(constant);
      DISPATCH();
    }
    DO_OP_NIL: push(NIL_VAL); DISPATCH();
    DO_OP_TRUE: push(BOOL_VAL(true)); DISPATCH();
    DO_OP_FALSE: push(BOOL_VAL(false)); DISPATCH();
    DO_OP_POP: pop(); DISPATCH();
    DO_OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      DISPATCH();
    }
    DO_OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      DISPATCH();
    }
    DO_OP_GET_GLOBAL: {
      ObjString* name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      DISPATCH();
    }
    DO_OP_DEFINE_GLOBAL: {
      ObjString* name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      DISPATCH();
    }
    DO_OP_SET_GLOBAL: {
      ObjString* name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name); 
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      DISPATCH();
    }
    DO_OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*frame->closure->upvalues[slot]->location);
      DISPATCH();
    }
    DO_OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek(0);
      DISPATCH();
    }
    DO_OP_GET_PROPERTY: {
      if(IS_ARRAY(peek(0)) || IS_STRING(peek(0))) {
        Obj* obj = AS_OBJ(peek(0));
        ObjString* name = READ_STRING();

        if(!bindNativeFn(obj, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        DISPATCH();
      }

      if (!IS_INSTANCE(peek(0))) {
        runtimeError("Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance* instance = AS_INSTANCE(peek(0));
      ObjString* name = READ_STRING();

      // Check properties first
      Value value;
      if (tableGet(&instance->fields, name, &value)) {
        pop(); // Instance.
        push(value);
        DISPATCH();
      }

      // otherwise bind method
      if (!bindMethod(instance->klass, name)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      DISPATCH();
    }
    DO_OP_SET_PROPERTY_SHADOWED:
    DO_OP_SET_PROPERTY: {
      if (!IS_INSTANCE(peek(1))) {
        runtimeError("Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }

      int opCode = *(frame->ip - 1);

      ObjInstance* instance = AS_INSTANCE(peek(1));
      tableSet(&instance->fields, READ_STRING(), peek(0));
      Value value = pop();
      pop();
      // Shadowed version passes the instance back, not the value assigned
      if(opCode == OP_SET_PROPERTY_SHADOWED) {
        push(OBJ_VAL(instance));
      }
      else {
        push(value);
      }
      DISPATCH();
    }
    DO_OP_GET_SUPER: {
      ObjString* name = READ_STRING();
      ObjClass* superclass = AS_CLASS(pop());

      if (!bindMethod(superclass, name)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      DISPATCH();
    }
    DO_OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      DISPATCH();
    }
    DO_OP_GREATER:  BINARY_OP(BOOL_VAL, >); DISPATCH();
    DO_OP_LESS:     BINARY_OP(BOOL_VAL, <); DISPATCH();
    DO_OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError(
            "Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      DISPATCH();
    }
    DO_OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); DISPATCH();
    DO_OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); DISPATCH();
    DO_OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); DISPATCH();
    DO_OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      DISPATCH();
    DO_OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      DISPATCH();
    DO_OP_PRINT: {
      printValue(pop());
      printf("\n");
      DISPATCH();
    }
    DO_OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      DISPATCH();
    }
    DO_OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0))) frame->ip += offset;
      DISPATCH();
    }
    DO_OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      DISPATCH();
    }
    DO_OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      DISPATCH();
    }
    DO_OP_INVOKE: {
      ObjString* method = READ_STRING();
      int argCount = READ_BYTE();
      if (!invoke(method, argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      DISPATCH();
    }
    DO_OP_SUPER_INVOKE: {
      ObjString* method = READ_STRING();
      int argCount = READ_BYTE();
      ObjClass* superclass = AS_CLASS(pop());
      if (!invokeFromClass(superclass, method, argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      DISPATCH();
    }
    DO_OP_CLOSURE: {
      ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure* closure = newClosure(function);
      push(OBJ_VAL(closure));
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] =
              captureUpvalue(frame->slots + index);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      DISPATCH();
    }
    DO_OP_CLOSE_UPVALUE:
      closeUpvalues(vm.stackTop - 1);
      pop();
      DISPATCH();
    DO_OP_RETURN: {
      Value result = pop();
      closeUpvalues(frame->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      push(result);
      frame = &vm.frames[vm.frameCount - 1];
      DISPATCH();
    }
    DO_OP_CLASS: {
      push(OBJ_VAL(newClass(READ_STRING())));
      DISPATCH();
    }
    DO_OP_INHERIT: {
      Value superclass = peek(1);
      if (!IS_CLASS(superclass)) {
        runtimeError("Superclass must be a class.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjClass* subclass = AS_CLASS(peek(0));
      tableAddAll(&AS_CLASS(superclass)->methods,
                  &subclass->methods);
      pop(); // Subclass.
      DISPATCH();
    }
    DO_OP_METHOD: {
      defineMethod(READ_STRING());
      DISPATCH();
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef DISPATCH
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
