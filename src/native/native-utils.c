#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "json/json.h"
#include "table.h"
#include "vm.h"

extern VM vm;

Value clockNative(Value *receiver, int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value parseRecurse(struct json_value_s *root) {
  if(json_value_as_object(root) != NULL) {
    Value objClassVal;
    tableGet(&vm.globals, copyString("Object", 6), &objClassVal);
    ObjInstance *instance = newInstance(AS_CLASS(objClassVal));
    push(OBJ_VAL(instance));
    struct json_object_s *object = json_value_as_object(root);
    struct json_object_element_s *element = object->start;
    while(element != NULL) {
      ObjString *key = copyString(element->name->string, element->name->string_size);
      push(OBJ_VAL(key));
      Value value = parseRecurse(element->value);
      push(value);
      tableSet(&instance->fields, key, value);
      pop();
      pop();
      element = element->next;
    }
    return pop(); // instance
  } else if(json_value_as_array(root) != NULL) {
    struct json_array_s *array = json_value_as_array(root);
    ObjArray *objArray = newArray();
    push(OBJ_VAL(objArray));
    struct json_array_element_s *element = array->start;
    while(element != NULL) {
      Value value = parseRecurse(element->value);
      push(value);
      writeValueArray(&objArray->values, value);
      pop();
      element = element->next;
    }
    return pop();
  } else if(json_value_as_string(root) != NULL) {
    struct json_string_s *string = json_value_as_string(root);
    return OBJ_VAL(copyString(string->string, string->string_size));
  } else if(json_value_as_number(root) != NULL) {
    struct json_number_s *number = json_value_as_number(root);
    return NUMBER_VAL(strtod(number->number, NULL));
  } else if(json_value_is_true(root)) {
    return TRUE_VAL;
  } else if(json_value_is_false(root)) {
    return FALSE_VAL;
  } else { // json_value_is_null(root)
    return NIL_VAL;
  }
}

Value parseJsonNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("parseJson() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("parseJson() argument must be a string.");
    return NIL_VAL;
  }
  ObjString *string = AS_STRING(args[0]);
  struct json_value_s* root = json_parse(string->chars, string->length);
  if(root == NULL) {
    // runtimeError("parseJson() failed to parse JSON.");
    return NIL_VAL;
  }
  Value result = parseRecurse(root);
  free(root);
  return result;
}

ObjString *stringifyRecurse(Value value) {
  if(IS_BOOL(value)) {
    return valuesEqual(TRUE_VAL, value) ? copyString("true", 4) : copyString("false", 5);
  } else if(IS_NUMBER(value)) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%.14g", AS_NUMBER(value));
    return copyString(buffer, length);
  } else if(IS_NIL(value)) {
    return copyString("null", 4);
  } else if(IS_STRING(value)) {
    push(OBJ_VAL(copyString("\"", 1)));
    push(value); // TODO escape quotes inside string
    push(OBJ_VAL(copyString("\"", 1)));
    concatenate();
    concatenate();
    return AS_STRING(pop());
  } else if(IS_ARRAY(value)) {
    ObjArray *array = AS_ARRAY(value);
    push(OBJ_VAL(copyString("[", 1)));
    for(int i = 0; i < array->values.count; i++) {
      ObjString *element = stringifyRecurse(array->values.values[i]);
      if(element == NULL) {
        // skip invalid values
        continue;
      }
      push(OBJ_VAL(element));
      concatenate();
      if(i != array->values.count - 1) {
        push(OBJ_VAL(copyString(", ", 2)));
        concatenate();
      }
    }
    push(OBJ_VAL(copyString("]", 1)));
    concatenate();
    return AS_STRING(pop());
  } else if(IS_INSTANCE(value)) {
    ObjInstance *instance = AS_INSTANCE(value);
    push(OBJ_VAL(copyString("{ ", 2)));
    Entry *entry = tableIterate(&instance->fields, NULL);
    int commas = 0;

    while(entry != NULL) {
      ObjString *key = entry->key;
      push(OBJ_VAL(copyString("\"", 1)));
      push(OBJ_VAL(key));
      push(OBJ_VAL(copyString("\": ", 3)));
      ObjString *element = stringifyRecurse(entry->value);
      if(element != NULL) {
        push(OBJ_VAL(element));
        concatenate();
        concatenate();
        concatenate(); 
        concatenate(); // concat whole stack
      } else {
        // skip invalid values
        // Pop off if not keeping
        pop();
        pop();
        pop();
      }


      entry = tableIterate(&instance->fields, entry);
      push(OBJ_VAL(copyString(", ", 2)));
      commas++;
    }
    if(commas > 1) {
      pop(); // pop off last comma
      for(int i = 0; i < commas - 1; i++) {
        // concat all the commas we added
        concatenate();
      }
    }
    push(OBJ_VAL(copyString(" }", 2)));
    concatenate();
    return AS_STRING(pop());
  } else {
    return NULL;
  }
}

Value stringifyJsonNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("parseJson() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  return OBJ_VAL(stringifyRecurse(args[0]));
}

Value scanToEOF(Value *receiver, int argCount, Value *args) {
  char line[1024];

  push(OBJ_VAL(copyString("", 0)));
  for(;;) {
    if(fgets(line, sizeof(line), stdin) == NULL) {
      break;
    }
    push(OBJ_VAL(copyString(line, strlen(line))));
    concatenate();
  }
  return pop();
}

Value getInstanceFields(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("getInstanceFields() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_INSTANCE(args[0])) {
    // runtimeError("getInstanceFields() argument must be an instance.");
    return NIL_VAL;
  }
  ObjInstance *instance = AS_INSTANCE(args[0]);
  ObjArray *objArray = newArray();
  push(OBJ_VAL(objArray));
  Entry *entry = tableIterate(&instance->fields, NULL);
  while(entry != NULL) {
    ObjString *key = entry->key;
    push(OBJ_VAL(key));
    writeValueArray(&objArray->values, OBJ_VAL(key));
    pop();
    entry = tableIterate(&instance->fields, entry);
  }
  return pop();
}

Value getInstanceFieldValueByKey(Value *receiver, int argCount, Value *args) {
  if(argCount != 2) {
    // runtimeError("getInstanceFieldValueByKey() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_INSTANCE(args[0])) {
    // runtimeError("getInstanceFieldValueByKey() first argument must be an instance.");
    return NIL_VAL;
  }
  if(!IS_STRING(args[1])) {
    // runtimeError("getInstanceFieldValueByKey() second argument must be a string.");
    return NIL_VAL;
  }
  ObjInstance *instance = AS_INSTANCE(args[0]);
  ObjString *key = AS_STRING(args[1]);
  Value value;
  if(tableGet(&instance->fields, key, &value)) {
    return value;
  } else {
    return NIL_VAL;
  }
}

Value printNative(Value *receiver, int argCount, Value *args) {
  for(int i = 0; i < argCount; i++) {
    printValue(args[i]);
    printf(" ");
  }
  return NIL_VAL;
}

Value printlnNative(Value *receiver, int argCount, Value *args) {
  printNative(receiver, argCount, args);
  printf("\n");
  return NIL_VAL;
}

Value sleepNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("sleep() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("sleep() argument must be a number.");
    return NIL_VAL;
  }
  double ms = AS_NUMBER(args[0]);
  sleep(ms / (double)1000);
  return NIL_VAL;
}

Value randNNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("randN() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("randN() first argument must be a number.");
    return NIL_VAL;
  }
  double max = AS_NUMBER(args[0]);
  int result = rand() % (int)max;
  return NUMBER_VAL((double)result);
}