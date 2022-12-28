#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include "../clox.h"

static scriptDirectory[PATH_MAX];

static char *getAbsolutePath(const char *path, int length) {
  char basePath[PATH_MAX];
  memcpy(basePath, path, length);
  basePath[length] = '\0';

  if(basePath[0] == '/') { // absolute path
    return strdup(basePath);
  }

  const *newPath = malloc(length + PATH_MAX + 1);
  strcpy(newPath, scriptDirectory);
  strcat(newPath, "/");
  strcat(newPath, basePath);
  return newPath;
}

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
  char *fname = getAbsolutePath(path->chars, path->length);
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
  char *fname = getAbsolutePath(path->chars, path->length);
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
  char *fname = getAbsolutePath(path->chars, path->length);
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
  char *fname = getAbsolutePath(path->chars, path->length);
  int result = remove(fname);
  free(fname);
  return BOOL_VAL(result == 0);
}

const char *getFileDescription(uint8_t type) {
  switch(type) {
    case DT_BLK: return "block_device";
    case DT_CHR: return "character_device";
    case DT_DIR: return "directory";
    case DT_FIFO: return "pipe";
    case DT_LNK: return "symlink";
    case DT_REG: return "file";
    case DT_SOCK: return "socket";
    case DT_UNKNOWN: return "unknown";
    default: return "invalid";
  }
}

Value listDirectory(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("listDirectory() takes exactly 1 argument (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("listDirectory() argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  char *fname = getAbsolutePath(path->chars, path->length);
  DIR *dir = opendir(fname);
  free(fname);
  if(dir == NULL) {
    // runtimeError("Could not open directory \"%s\"", path->chars);
    return NIL_VAL;
  }
  Value objectClass;
  if(!tableGet(&vm.globals, copyString("Object", 6), &objectClass)) {
    closedir(dir);
    // runtimeError("Could not find class Object");
    return NIL_VAL;
  }
  ObjArray *list = newArray();
  ObjString *nameKey = copyString("name", 4);
  ObjString *typeKey = copyString("type", 4);
  push(OBJ_VAL(list));
  push(OBJ_VAL(nameKey));
  push(OBJ_VAL(typeKey));
  struct dirent *entry;
  while((entry = readdir(dir)) != NULL) {
    ObjInstance *obj = newInstance(AS_CLASS(objectClass));
    push(OBJ_VAL(obj));

    ObjString *name = copyString(entry->d_name, strlen(entry->d_name));
    push(OBJ_VAL(name));
    tableSet(&obj->fields, nameKey, OBJ_VAL(name));
    pop();

    const char *description = getFileDescription(entry->d_type);
    ObjString *type = copyString(description, strlen(description));
    push(OBJ_VAL(type));
    tableSet(&obj->fields, typeKey, OBJ_VAL(type));
    pop();

    writeValueArray(&list->values, peek(0));

    pop();
  }
  pop();
  pop();
  pop();
  closedir(dir);
  return OBJ_VAL(list);
}

Value fileExists(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("fileExists() takes exactly 1 argument (%d given)", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("fileExists() argument must be a string");
    return NIL_VAL;
  }
  ObjString *path = AS_STRING(args[0]);
  char *fname = getAbsolutePath(path->chars, path->length);
  int result = access(fname, F_OK);
  free(fname);
  return BOOL_VAL(result == 0);
}

bool registerModule() {
  if(vm.scriptName[0] != '\0') {
    char *scriptName = strdup(vm.scriptName);
    strcpy(scriptDirectory, dirname(scriptName));
    free(scriptName);
  }
  else {
    strcpy(scriptDirectory, vm.workingDirectory);
  }

  registerNativeMethod("readFileText", readFileText);
  registerNativeMethod("writeFileText", writeFileText);
  registerNativeMethod("appendFileText", appendFileText);
  registerNativeMethod("deleteFile", deleteFile);
  registerNativeMethod("listDirectory", listDirectory);
  registerNativeMethod("fileExists", fileExists);
  return true;
}