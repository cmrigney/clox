#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjString* key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

EXPORT void freeTable(Table* table);
EXPORT void initTable(Table* table);
EXPORT bool tableGet(Table* table, ObjString* key, Value* value);
EXPORT bool tableSet(Table* table, ObjString* key, Value value);
EXPORT bool tableDelete(Table* table, ObjString* key);
EXPORT void tableAddAll(Table* from, Table* to);
EXPORT ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);
EXPORT void tableRemoveWhite(Table* table);
EXPORT void markTable(Table* table);
EXPORT Entry *tableIterate(Table *table, Entry *previous);

#endif
