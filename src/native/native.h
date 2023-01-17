#ifndef clox_native_h
#define clox_native_h

#include "value.h"

Value clockNative(Value *receiver, int argCount, Value* args);
Value randNNative(Value *receiver, int argCount, Value *args);

Value parseJsonNative(Value *receiver, int argCount, Value *args);
Value stringifyJsonNative(Value *receiver, int argCount, Value *args);

Value scanToEOF(Value *receiver, int argCount, Value *args);
Value printNative(Value *receiver, int argCount, Value *args);
Value printlnNative(Value *receiver, int argCount, Value *args);
Value printMethods(Value *receiver, int argCount, Value *args);

Value getEnvVarNative(Value *receiver, int argCount, Value *args);

Value getMemStatsNative(Value *receiver, int argCount, Value *args);

Value getInstanceFields(Value *receiver, int argCount, Value *args);
Value getInstanceFieldValueByKey(Value *receiver, int argCount, Value *args);
Value setInstanceFieldValueByKey(Value *receiver, int argCount, Value *args);

// Array methods
Value array(Value *receiver, int argCount, Value* args);
Value array_count(Value *receiver, int argCount, Value *args);
Value array_push(Value *receiver, int argCount, Value *args);
Value array_pop(Value *receiver, int argCount, Value *args);
Value array_get(Value *receiver, int argCount, Value *args);
Value array_filter(Value *receiver, int argCount, Value *args);
Value array_map(Value *receiver, int argCount, Value *args);
Value array_foreach(Value *receiver, int argCount, Value *args);
Value array_slice(Value *receiver, int argCount, Value *args);

// Buffer methods
Value bufferConstructor(Value *receiver, int argCount, Value* args);
Value buffer_length(Value *receiver, int argCount, Value* args);
Value buffer_get(Value *receiver, int argCount, Value* args);
Value buffer_set(Value *receiver, int argCount, Value* args);
Value buffer_as_array(Value *receiver, int argCount, Value* args);
Value buffer_as_string(Value *receiver, int argCount, Value* args);
Value buffer_append(Value *receiver, int argCount, Value* args);

// String methods
Value string_length(Value *receiver, int argCount, Value* args);
Value string_get(Value *receiver, int argCount, Value* args);
Value string_find(Value *receiver, int argCount, Value* args);
Value string_substring(Value *receiver, int argCount, Value* args);
Value string_split(Value *receiver, int argCount, Value* args);
Value string_replace(Value *receiver, int argCount, Value* args);

#endif