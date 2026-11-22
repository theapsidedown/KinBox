#pragma once
#include <stdint.h>
#include <stdlib.h>
static inline uint32_t sys_rand32_get(void){ return ((uint32_t)rand()<<16) ^ (uint32_t)rand(); }
