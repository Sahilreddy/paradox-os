
// ParadoxOS Type Definitions

#ifndef TYPES_H
#define TYPES_H

// Standard integer types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// Size types
typedef unsigned long size_t;
typedef signed long ssize_t;

// Pointer types
typedef unsigned long uintptr_t;
typedef signed long intptr_t;

// Boolean (use built-in bool in C++)
#ifdef __cplusplus
// C++ has built-in bool
#else
typedef int bool;
#define true 1
#define false 0
#endif

// NULL
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif // TYPES_H
