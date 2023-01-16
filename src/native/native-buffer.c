#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

static void memcpy_char_uint8_t(uint8_t *dest, char *src, int size) {
  for(int i = 0; i < size; i++) {
    dest[i] = (uint8_t)src[i];
  }
}

static void memcpy_uint8_t_char(char *dest, uint8_t *src, int size) {
  for(int i = 0; i < size; i++) {
    dest[i] = (char)src[i];
  }
}

Value bufferConstructor(Value *receiver, int argCount, Value* args) {
  ObjBuffer *value;
  if(argCount == 1 && IS_NUMBER(args[0])) {
    value = newBuffer((int)AS_NUMBER(args[0]));
    memset(value->bytes, 0, value->size);
  } else if(argCount == 1 && IS_BUFFER(args[0])) {
    value = newBuffer(AS_BUFFER(args[0])->size);
    memcpy(value->bytes, AS_BUFFER(args[0])->bytes, value->size);
  } else if(argCount == 1 && IS_ARRAY(args[0])) {
    value = newBuffer(AS_ARRAY(args[0])->values.count);
    for(int i = 0; i < AS_ARRAY(args[0])->values.count; i++) {
      Value val = AS_ARRAY(args[0])->values.values[i];
      if(IS_NUMBER(val) && (uint8_t)AS_NUMBER(val) <= 255 && (uint8_t)AS_NUMBER(val) >= 0) {
        value->bytes[i] = (uint8_t)AS_NUMBER(val);
      } else {
        // runtimeError("Array contains value that is not a number <= 255 and >= 0.");
        return NIL_VAL;
      }
    }
  } else if(argCount == 1 && IS_STRING(args[0])) {
    value = newBuffer(AS_STRING(args[0])->length);
    memcpy_char_uint8_t(value->bytes, AS_STRING(args[0])->chars, value->size);
  } else {
    value = newBuffer(0);
  }
  return OBJ_VAL(value);
}

Value buffer_length(Value *receiver, int argCount, Value* args) {
  if (argCount != 0) {
    // runtimeError("Expected 0 arguments for 'length' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  return NUMBER_VAL(buffer->size);
}

Value buffer_get(Value *receiver, int argCount, Value* args) {
  if (argCount != 1) {
    // runtimeError("Expected 1 argument for 'get' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("Index is not a number.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  int index = (int)AS_NUMBER(args[0]);
  if(index < 0 || index >= buffer->size) {
    // runtimeError("Index out of bounds.");
    return NIL_VAL;
  }
  return NUMBER_VAL((double)buffer->bytes[index]);
}

Value buffer_set(Value *receiver, int argCount, Value* args) {
  if (argCount != 2) {
    // runtimeError("Expected 2 arguments for 'set' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("Index is not a number.");
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[1])) {
    // runtimeError("Value is not a number.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  int index = (int)AS_NUMBER(args[0]);
  if(index < 0 || index >= buffer->size) {
    // runtimeError("Index out of bounds.");
    return NIL_VAL;
  }
  buffer->bytes[index] = (uint8_t)AS_NUMBER(args[1]);
  return NIL_VAL;
}

Value buffer_as_array(Value *receiver, int argCount, Value* args) {
  if (argCount != 0) {
    // runtimeError("Expected 0 arguments for 'asArray' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  ObjArray *array = newArray();
  push(OBJ_VAL(array));
  for(int i = 0; i < buffer->size; i++) {
    writeValueArray(&array->values, NUMBER_VAL(buffer->bytes[i]));
  }
  pop();
  return OBJ_VAL(array);
}

Value buffer_as_string(Value *receiver, int argCount, Value* args) {
  if (argCount != 0) {
    // runtimeError("Expected 0 arguments for 'asString' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  char* heapChars = ALLOCATE(char, buffer->size + 1);
  memcpy_uint8_t_char(heapChars, buffer->bytes, buffer->size);
  heapChars[buffer->size] = '\0';
  ObjString *string = takeString(heapChars, buffer->size);
  return OBJ_VAL(string);
}

Value buffer_append(Value *receiver, int argCount, Value* args) {
  if (argCount != 1) {
    // runtimeError("Expected 1 argument for 'append' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_BUFFER(*receiver)) {
    // runtimeError("Value is not a buffer.");
    return NIL_VAL;
  }
  if(!IS_BUFFER(args[0])) {
    // runtimeError("Argument is not a buffer.");
    return NIL_VAL;
  }
  ObjBuffer *buffer = AS_BUFFER(*receiver);
  ObjBuffer *other = AS_BUFFER(args[0]);
  buffer->bytes = GROW_ARRAY(uint8_t, buffer->bytes, buffer->size, buffer->size + other->size);
  memcpy(buffer->bytes + buffer->size, other->bytes, other->size);
  buffer->size += other->size;
  return OBJ_VAL(buffer);
}
