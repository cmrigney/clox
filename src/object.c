#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;

  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

  return object;
}

ObjArray* newArray() {
  ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
  initValueArray(&array->values);
  return array;
}

ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
                                       OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass* newClass(ObjString* name) {
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  initTable(&klass->methods);
  return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*,
                                   function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance* newInstance(ObjClass* klass) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjNative* newNative(NativeFn function, bool callsLox) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  native->callsLox = callsLox;
  return native;
}

ObjBoundNative* newBoundNative(Value receiver, NativeFn function, bool callsLox) {
  ObjBoundNative* native = ALLOCATE_OBJ(ObjBoundNative, OBJ_BOUND_NATIVE);
  native->function = function;
  native->receiver = receiver;
  native->callsLox = callsLox;
  return native;
}

static ObjString* allocateString(char* chars, int length,
                                 uint32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string)); // for garbage collection safety
  tableSet(&vm.strings, string, NIL_VAL);
  pop();

  return string;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

// Assumes ownership of chars
ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

// Assumes it cannot take ownership of chars
ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(FILE *stream, ObjFunction* function) {
  if (function->name == NULL) {
    fprintf(stream, "<script>");
    return;
  }
  fprintf(stream, "<fn %s>", function->name->chars);
}

void printObjectInstance(FILE *stream, ObjInstance *instance) {
  printf(".{ ");
  Entry *entry = tableIterate(&instance->fields, NULL);
  while(entry != NULL) {
    printObject(stream, OBJ_VAL(entry->key));
    fprintf(stream, ": ");
    printValue(stream, entry->value);
    fprintf(stream, ", ");

    entry = tableIterate(&instance->fields, entry);
  }
  printf("}");
}

void printObject(FILE *stream, Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
      printFunction(stream, AS_BOUND_METHOD(value)->method->function);
      break;
    case OBJ_CLASS:
      fprintf(stream, "%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(stream, AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(stream, AS_FUNCTION(value));
      break;
    case OBJ_INSTANCE: {
      int instanceLen = AS_INSTANCE(value)->klass->name->length;
      int objLen = (int)strlen("Object");
      if(instanceLen == objLen && strcmp(AS_INSTANCE(value)->klass->name->chars, "Object") == 0) {
        printObjectInstance(stream, AS_INSTANCE(value));
      }
      else {
        fprintf(stream, "%s instance",
              AS_INSTANCE(value)->klass->name->chars);
      }
      break;
    }
    case OBJ_NATIVE:
      fprintf(stream, "<native fn>");
      break;
    case OBJ_BOUND_NATIVE:
      fprintf(stream, "<bound native fn>");
      break;
    case OBJ_STRING:
      fprintf(stream, "%s", AS_CSTRING(value));
      break;
    case OBJ_UPVALUE:
      fprintf(stream, "upvalue");
      break;
    case OBJ_ARRAY:
      fprintf(stream, "Array(");
      for(int i = 0; i < AS_ARRAY(value)->values.count; i++) {
        printValue(stream, AS_ARRAY(value)->values.values[i]);
        if(i < AS_ARRAY(value)->values.count - 1) {
          fprintf(stream, ", ");
        }
      }
      fprintf(stream, ")");
      break;
  }
}
