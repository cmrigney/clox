#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "../clox.h"

Value readFileText(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("readFile() takes exactly 1 argument (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("readFile() argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  char *fname = malloc(path->length + 1);
  memcpy(fname, path->chars, path->length);
  fname[path->length] = '\0';
  FILE *file = fopen(fname, "r");
  free(fname);
  if(file == NULL) {
    // runtimeError("Could not open file \"%s\"", path->chars);
    return NIL_VAL;
  }
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);
  char *buffer = malloc(size + 1);
  if(buffer == NULL) {
    // runtimeError("Could not allocate memory for file \"%s\"", path->chars);
    fclose(file);
    return NIL_VAL;
  }
  size_t read = fread(buffer, sizeof(char), size, file);
  if(read < size) {
    // runtimeError("Could not read file \"%s\"", path->chars);
    free(buffer);
    fclose(file);
    return NIL_VAL;
  }
  buffer[size] = '\0';
  fclose(file);
  return OBJ_VAL(takeString(buffer, size));
}

Value writeFileText(Value *receiver, int argCount, Value *args) {
  if(argCount != 2) {
    // runtimeError("writeFile() takes exactly 2 arguments (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("writeFile() first argument must be a string");
    return NIL_VAL;
  }
  if(!IS_STRING(args[1])) {
    // runtimeError("writeFile() second argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  ObjString *text = AS_STRING(args[1]);
  char *fname = malloc(path->length + 1);
  memcpy(fname, path->chars, path->length);
  fname[path->length] = '\0';
  FILE *file = fopen(fname, "w");
  free(fname);
  if(file == NULL) {
    return FALSE_VAL;
  }
  size_t written = fwrite(text->chars, sizeof(char), text->length, file);
  if(written < text->length) {
    fclose(file);
    return FALSE_VAL;
  }
  fclose(file);
  return TRUE_VAL;
}

Value appendFileText(Value *receiver, int argCount, Value *args) {
  if(argCount != 2) {
    // runtimeError("appendFile() takes exactly 2 arguments (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("appendFile() first argument must be a string");
    return NIL_VAL;
  }
  if(!IS_STRING(args[1])) {
    // runtimeError("appendFile() second argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  ObjString *text = AS_STRING(args[1]);
  char *fname = malloc(path->length + 1);
  memcpy(fname, path->chars, path->length);
  fname[path->length] = '\0';
  FILE *file = fopen(fname, "a");
  free(fname);
  if(file == NULL) {
    return FALSE_VAL;
  }
  size_t written = fwrite(text->chars, sizeof(char), text->length, file);
  if(written < text->length) {
    fclose(file);
    return FALSE_VAL;
  }
  fclose(file);
  return TRUE_VAL;
}

Value deleteFile(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("deleteFile() takes exactly 1 argument (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("deleteFile() argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  char *fname = malloc(path->length + 1);
  memcpy(fname, path->chars, path->length);
  fname[path->length] = '\0';
  int result = remove(fname);
  free(fname);
  return BOOL_VAL(result == 0);
}

bool registerModule() {
  registerNativeMethod("readFileText", readFileText);
  registerNativeMethod("writeFileText", writeFileText);
  registerNativeMethod("appendFileText", appendFileText);
  registerNativeMethod("deleteFile", deleteFile);
  return true;
}