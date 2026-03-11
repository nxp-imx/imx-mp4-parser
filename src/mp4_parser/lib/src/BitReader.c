/*
 ***********************************************************************
 * Copyright 2021-2022, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

//#define DEBUG

#include "BitReader.h"
#include "W32FileMappingObject.h"
#include "MP4Impl.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static size_t BitReaderNumBitsLeft(BitReader* this) {
    return (this->size - this->bytePos - 1) * 8 + 8 - this->bitPos;
}

static bool getBitsInternal(BitReader* this, size_t n, uint32* out) {
    uint32 result = 0;
    size_t readBitsInCurByte = 0;
    size_t bitsInCurByte = 0;
    uint32 intC;

    if (n > 32) {
        return FALSE;
    }

    while (n > 0) {
        // cur byte finished, move to next
        if (this->bitPos == 8) {
            this->bytePos++;
            this->bitPos = 0;
            if (this->bytePos >= this->size)
                return FALSE;
        }
        bitsInCurByte = 8 - this->bitPos;
        readBitsInCurByte = MIN(bitsInCurByte, n);
        intC = (uint32)this->data[this->bytePos];
        result = (result << readBitsInCurByte) |
                 ((intC << this->bitPos) & 0xFF) >> (8 - readBitsInCurByte);
        n -= readBitsInCurByte;
        this->bitPos += readBitsInCurByte;
    }

    *out = result;
    return TRUE;
}

static uint32 BitReaderGetBits(BitReader* this, size_t n) {
    uint32 ret = 0;
    if (FALSE == getBitsInternal(this, n, &ret)) {
        MP4MSG("getBits: fail for n: %zu", n);
    }
    return ret;
}

static bool BitReaderSkipBits(BitReader* this, size_t n) {
    uint32 dummy;
    size_t m;
    while (n > 0) {
        m = MIN(n, 32);
        if (!getBitsInternal(this, m, &dummy)) {
            return FALSE;
        }
        n -= m;
    }
    return TRUE;
}

void BitReaderInit(BitReader* this, const uint8_t* data, size_t size) {
    this->data = data;
    this->size = size;
    this->bitPos = 0;
    this->bytePos = 0;
    this->getBits = BitReaderGetBits;
    this->skipBits = BitReaderSkipBits;
    this->numBitsLeft = BitReaderNumBitsLeft;
}
