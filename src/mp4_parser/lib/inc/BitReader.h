/*
 ***********************************************************************
 * Copyright 2021, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#ifndef BIT_READER_H_
#define BIT_READER_H_

#include <sys/types.h>
#include "fsl_types.h"
#include "utils.h"

typedef struct _BitReader {
    const uint8* data;
    size_t size;
    size_t bitPos;   // reading position of bit in current byte: 0 ~ 7
    size_t bytePos;  // reading position of current byte in data: 0 ~ size-1

    // interfaces
    uint32 (*getBits)(struct _BitReader* this, size_t n);
    bool (*skipBits)(struct _BitReader* this, size_t n);
    size_t (*numBitsLeft)(struct _BitReader* this);
} BitReader;

void BitReaderInit(BitReader* this, const uint8* data, size_t size);

#endif  // A_BIT_READER_H_
