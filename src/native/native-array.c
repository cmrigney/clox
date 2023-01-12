#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

Value array(Value *receiver, int argCount, Value* args) {
  ObjArray *value = newArray();
  ValueArray *arr = &value->values;
  push(OBJ_VAL(value));
  for (int i = 0; i < argCount; i++) {
    writeValueArray(arr, args[i]);
  }
  pop();
  return OBJ_VAL(value);
}

Value array_count(Value *receiver, int argCount, Value *args) {
  if (argCount != 0) {
    // runtimeError("Expected 0 arguments for 'count' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_ARRAY(*receiver)) {
    // runtimeError("Value is not an array.");
    return NIL_VAL;
  }
  ObjArray *array = AS_ARRAY(*receiver);
  return NUMBER_VAL(array->values.count);
}

Value array_push(Value *receiver, int argCount, Value *args) {
  if (argCount != 1) {
    // runtimeError("Expected 1 argument for 'push' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_ARRAY(*receiver)) {
    // runtimeError("Value is not an array.");
    return NIL_VAL;
  }
  ObjArray *array = AS_ARRAY(*receiver);
  writeValueArray(&array->values, args[0]);
  return OBJ_VAL(array);
}

Value array_pop(Value *receiver, int argCount, Value *args) {
  if (argCount != 0) {
    // runtimeError("Expected 0 arguments for 'pop' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_ARRAY(*receiver)) {
    // runtimeError("Value is not an array.");
    return NIL_VAL;
  }
  ObjArray *array = AS_ARRAY(*receiver);
  if (array->values.count == 0) {
    return NIL_VAL;
  }
  return array->values.values[--array->values.count];
}

Value array_get(Value *receiver, int argCount, Value *args) {
  if (argCount != 1) {
    // runtimeError("Expected 1 argument for 'get' but got %d.", argCount);
    return NIL_VAL;
  }
  if(!IS_ARRAY(*receiver)) {
    // runtimeError("Value is not an array.");
    return NIL_VAL;
  }
  ObjArray *array = AS_ARRAY(*receiver);
  if (!IS_NUMBER(args[0])) {
    // runtimeError("Index must be a number.");
    return NIL_VAL;
  }
  int index = AS_NUMBER(args[0]);
  if (index < 0 || index >= array->values.count) {
    // runtimeError("Index out of bounds.");
    return NIL_VAL;
  }
  return array->values.values[index];
}

Value array_filter(Value *receiver, int argCount, Value *args) {
  // TODO verify args
  return callLoxCode("_array_filter", receiver, argCount, args);
}

Value array_map(Value *receiver, int argCount, Value *args) {
  // TODO verify args
  return callLoxCode("_array_map", receiver, argCount, args);
}

Value array_foreach(Value *receiver, int argCount, Value *args) {
  // TODO verify args
  return callLoxCode("_array_foreach", receiver, argCount, args);
}

Value array_slice(Value *receiver, int argCount, Value *args) {
  // TODO verify args
  return callLoxCode("_array_slice", receiver, argCount, args);
}
