#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "json/json.h"
#include "table.h"
#include "vm.h"

Value string_length(Value *receiver, int argCount, Value* args) {
  return NUMBER_VAL((double)AS_STRING(*receiver)->length);
}

Value string_get(Value *receiver, int argCount, Value* args) {
  if(argCount != 1) {
    // runtimeError("String.get() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("String.get() argument must be a number.");
    return NIL_VAL;
  }
  int index = AS_NUMBER(args[0]);
  if(index < 0 || index >= AS_STRING(*receiver)->length) {
    // runtimeError("String.get() index out of bounds.");
    return NIL_VAL;
  }
  return OBJ_VAL(copyString(AS_STRING(*receiver)->chars + index, 1));
}

Value string_find(Value *receiver, int argCount, Value* args) {
  if(argCount != 1) {
    // runtimeError("String.find() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("String.find() argument must be a string.");
    return NIL_VAL;
  }
  ObjString *needle = AS_STRING(args[0]);
  char *result = strstr(AS_STRING(*receiver)->chars, needle->chars);
  if(result == NULL) {
    return NIL_VAL;
  }
  return NUMBER_VAL((double)(result - AS_STRING(*receiver)->chars));
}

Value string_substring(Value *receiver, int argCount, Value* args) {
  if(argCount != 2) {
    // runtimeError("String.substring() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
    // runtimeError("String.substring() arguments must be numbers.");
    return NIL_VAL;
  }
  int start = (int)AS_NUMBER(args[0]);
  int length = (int)AS_NUMBER(args[1]);
  ObjString *string = AS_STRING(*receiver);
  if(start < 0 || start >= string->length) {
    // runtimeError("String.substring() start index out of bounds.");
    return NIL_VAL;
  }
  int maxLength = string->length - start;
  if(start + length > string->length) {
    length = maxLength;
  }
  if(length < 0) {
    // runtimeError("String.substring() length out of bounds.");
    return NIL_VAL;
  }
  return OBJ_VAL(copyString(string->chars + start, length));
}

Value string_split(Value *receiver, int argCount, Value* args) {
  if(argCount != 1) {
    // runtimeError("String.split() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("String.split() argument must be a string.");
    return NIL_VAL;
  }
  if(AS_STRING(args[0])->length == 0) {
    ObjArray *array = newArray();
    push(OBJ_VAL(array));
    writeValueArray(&array->values, *receiver);
    pop();
    return OBJ_VAL(array);
  }
  ObjString *delimiter = AS_STRING(args[0]);
  ObjString *string = AS_STRING(*receiver);
  ObjArray *array = newArray();
  push(OBJ_VAL(array));
  char *start = string->chars;
  char *end = strstr(start, delimiter->chars);
  while(end != NULL) {
    Value substring = OBJ_VAL(copyString(start, end - start));
    push(substring);
    writeValueArray(&array->values, substring);
    pop();
    start = end + delimiter->length;
    end = strstr(start, delimiter->chars);
  }
  Value endString = OBJ_VAL(copyString(start, string->length - (start - string->chars)));
  push(endString);
  writeValueArray(&array->values, endString);
  pop();
  pop();
  return OBJ_VAL(array);
}

Value string_replace(Value *receiver, int argCount, Value* args) {
  return callLoxCode("_string_replace", receiver, argCount, args);
}
