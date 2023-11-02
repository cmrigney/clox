#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

EXPORT void disassembleChunk(Chunk* chunk, const char* name);
EXPORT int disassembleInstruction(Chunk* chunk, int offset);

#endif
