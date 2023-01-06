#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "../clox.h"

static Value sleepMsNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("sleepMs() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("sleepMs() argument must be a number.");
    return NIL_VAL;
  }
  double ms = AS_NUMBER(args[0]);
  sleep_ms(ms);
  return NIL_VAL;
}

bool registerModule_pico() {
  stdio_init_all();
  registerNativeMethod("sleep", sleepMsNative);
  return true;
}

static bool readline(char *buffer, size_t bufferLength) {
  char u, *p;
  for(p = buffer, u = getchar(); tud_cdc_connected() && u !='|' && u != EOF && p - buffer < bufferLength - 1; u = getchar()) {
    *p++ = u;
  }
  *p = 0;
  if(u == '|') {
    putchar('\n');
  }
  return tud_cdc_connected() && u != EOF;
}

void pico_repl() {
  char line[1024];
  interpret("var pico = systemImport(\"pico\");");
  for (;;) {
    while (!tud_cdc_connected()) { sleep_ms(100);  }

    printf("> ");

    if (!readline(line, sizeof(line))) {
      printf("\n");
      continue;
    }

    interpret(line);
  }
}