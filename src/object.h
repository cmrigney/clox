#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_ARRAY(value)        isObjType(value, OBJ_ARRAY)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_BOUND_NATIVE(value) isObjType(value, OBJ_BOUND_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_BUFFER(value)       isObjType(value, OBJ_BUFFER)
#define IS_REF(value)          isObjType(value, OBJ_REF)

#define AS_ARRAY(value)        ((ObjArray*)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_BOUND_NATIVE(value) \
    ((ObjBoundNative*)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_BUFFER(value)       ((ObjBuffer*)AS_OBJ(value))
#define AS_REF(value)          ((ObjRef*)AS_OBJ(value))

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_INSTANCE,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
  OBJ_ARRAY,
  OBJ_BOUND_NATIVE,
  OBJ_BUFFER,
  OBJ_REF,
} ObjType;

struct Obj {
  ObjType type;
  bool isMarked;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(Value *receiver, int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
  bool callsLox;
} ObjNative;

typedef struct {
  Obj obj;
  Value receiver;
  NativeFn function;
  bool callsLox;
} ObjBoundNative;

struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  int size;
  uint8_t* bytes;
} ObjBuffer;

typedef struct ObjRef {
  Obj obj;
  const char *magic; // object type
  const char *description;
  void *data;
  void (*dispose)(void *data);
} ObjRef;

typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct {
  Obj obj;
  ObjString* name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass* klass;
  Table fields; 
} ObjInstance;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod;

typedef struct {
  Obj obj;
  ValueArray values;
} ObjArray;

ObjArray *newArray();
ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function, bool callsLox);
ObjBoundNative* newBoundNative(Value receiver, NativeFn function, bool callsLox);
ObjString* takeString(char* chars, int length);
ObjBuffer* newBuffer(int size);
ObjBuffer* takeBuffer(uint8_t* bytes, int size);
ObjRef* newRef(const char *magic, const char *description, void *data, void (*dispose)(void *data));

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
