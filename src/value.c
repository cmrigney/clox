#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values,
                               oldCapacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void printValue(FILE *stream, Value value) {
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    fprintf(stream, AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    fprintf(stream, "nil");
  } else if (IS_NUMBER(value)) {
    fprintf(stream, "%g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(stream, value);
  }
#else
  switch (value.type) {
    case VAL_BOOL:
      fprintf(stream, AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: fprintf(stream, "nil"); break;
    case VAL_NUMBER: fprintf(stream, "%g", AS_NUMBER(value)); break;
    case VAL_OBJ: printObject(stream, value); break;
  }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
#else
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
    default:         return false; // Unreachable.
  }
#endif
}
