#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

// Need to save as much memory as possible on low mem system
#ifdef PICO_MODULE
#define DEBUG_STRESS_GC
#endif

#endif
