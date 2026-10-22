#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define RF_SIG_BUF_SIZE 1024
typedef struct {
    uint32_t buf[RF_SIG_BUF_SIZE];
    size_t   count;
    bool     ready;
} RfSig;
