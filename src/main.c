#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef EMCC_JS
#include <emscripten.h>
#endif

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "jit/jit.h"

#include "stdlib_lox.h"
#ifdef BUNDLE
#include "bundle_lox.h"
#endif

#ifdef PICO_MODULE
#include "pico/clox-pico.h"
#endif

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\": %d.\n", path, errno);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

void runtime_exit(int exit_code) {
  #ifdef PICO_MODULE
  pico_exit(exit_code);
  #else
  exit(exit_code);
  #endif
}

static void runBytes(const unsigned char *bytes, unsigned int length) {
  char* source = (char*)malloc(length + 1);
  memcpy(source, bytes, length);
  source[length] = '\0';
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) runtime_exit(70);
}

static void runFile(const char* path, bool validateOnly) {
  char* source = readFile(path);
  strcpy(vm.scriptName, path);

  InterpretResult result;
  if(validateOnly) {
    result = validate(source);
  } else {
    if(getenv("CLOX_JIT")) {
      result = compileAndRun(source);
    }
    else {
      result = interpret(source);
    }
  }
  free(source); 
  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) runtime_exit(70);
}

static void setupStdLib() {
  runBytes(stdlib_lib_lox, stdlib_lib_lox_len);
}

int main(int argc, const char* argv[]) {
  #ifdef EMCC_JS
    // EM_ASM is a macro to call in-line JavaScript code.
    EM_ASM(
      FS.mkdir('/app');
      FS.mount(NODEFS, { root: '.' }, '/app');
    );
  #endif

  srand((unsigned int)clock());

  initVM();

  #ifdef BUNDLE
  setupStdLib();
  runBytes(exec_bundle, exec_bundle_len);
  #elif defined (PICO_MODULE)
  setupStdLib();
  pico_repl();
  #else
  if (argc == 1) {
    setupStdLib();
    repl();
  } else if (argc == 2) {
    // setupStdLib();
    runFile(argv[1], false);
  } else if(argc == 3 && strcmp(argv[1], "validate") == 0) {
    // setupStdLib();
    runFile(argv[2], true);
    fprintf(stdout, "%s is valid\n", argv[2]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }
  #endif

  freeVM();
  #ifdef PICO_MODULE
  unregisterModule_pico();
  #endif
  return 0;
}