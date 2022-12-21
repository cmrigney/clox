#ifndef clox_native_h
#define clox_native_h

#include "value.h"

Value clockNative(Value *receiver, int argCount, Value* args);

Value parseJsonNative(Value *receiver, int argCount, Value *args);
Value stringifyJsonNative(Value *receiver, int argCount, Value *args);

Value scanToEOF(Value *receiver, int argCount, Value *args);

// Array methods
Value array(Value *receiver, int argCount, Value* args);
Value array_count(Value *receiver, int argCount, Value *args);
Value array_push(Value *receiver, int argCount, Value *args);
Value array_pop(Value *receiver, int argCount, Value *args);
Value array_get(Value *receiver, int argCount, Value *args);
Value array_filter(Value *receiver, int argCount, Value *args);
Value array_map(Value *receiver, int argCount, Value *args);

#endif