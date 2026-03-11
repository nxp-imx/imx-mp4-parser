/*
 ***********************************************************************
 * Copyright (c) 2011 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include "MP4Atoms.h"

int32 MP4StringisUTF8(const int8* str, int32 len);
MP4Err MP4ConvertUTF16BEtoUTF8(const uint16** sourceStart, const uint16* sourceEnd,
                               uint8** targetStart, uint8* targetEnd);
MP4Err MP4ConvertASCIItoUTF8(const uint8* sourceStart, uint32 sourceSize, uint8* targetStart,
                             uint32* targetSize);
