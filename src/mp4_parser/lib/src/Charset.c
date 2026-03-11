/*
 ***********************************************************************
 * Copyright (c) 2011-2015, Freescale Semiconductor, Inc.
 *
 * Copyright 2018, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include "MP4Atoms.h"

#define UNI_SUR_HIGH_START ((uint32)0xD800)
#define UNI_SUR_HIGH_END ((uint32)0xDBFF)
#define UNI_SUR_LOW_START ((uint32)0xDC00)
#define UNI_SUR_LOW_END ((uint32)0xDFFF)

#define HALF_SHIFT 10
#define HALF_BASE 0x0010000UL

#define UNI_REPLACEMENT_CHAR ((uint32)0x0000FFFD)

#define NEXT_CHAR                                       \
    do {                                                \
        if ((*(uint8*)p & 0xC0) != 0x80) /* 10xxxxxx */ \
            goto error;                                 \
        val <<= 6;                                      \
        val |= (*(uint8*)p) & 0x3F;                     \
    } while (0)

#define IS_VALID_UNICODE(char)                               \
    ((char) < 0x110000 && (((char)&0xFFFFF800) != 0xD800) && \
     ((char) < 0xFDD0 || (char) > 0xFDEF) && ((char)&0xFFFE) != 0xFFFE)

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const uint8 g_firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

int32 MP4StringisUTF8(const int8* str, int32 len) {
    uint32 val = 0;
    uint32 min = 0;
    const int8* p;

    // fix the bug when last byte is \0 in string.
    p = str + len;
    if (*p == '\0' && len > 1)
        len -= 1;

    for (p = str; ((p - str) < len) && *p; p++) {
        if (*(uint8*)p >= 128) {
            const int8* last;

            last = p;
            if ((*(uint8*)p & 0xE0) == 0xC0) /* 110x xxxx */
            {
                if ((len - (p - str) < 2) || ((*(uint8*)p & 0xC0) == 0)) {
                    goto error;
                }

                p++;

                if ((*(uint8*)p & 0xC0) != 0x80) /* 10xx xxxx */
                {
                    goto error;
                }
            } else {
                if ((*(uint8*)p & 0xF0) == 0xE0) /* 1110 xxxx */
                {
                    if (len - (p - str) < 3) {
                        goto error;
                    }

                    min = (1 << 11);
                    val = *(uint8*)p & 0x0F;
                    goto two_remain;
                } else if ((*(uint8*)p & 0xF8) == 0xF0) /* 1111 0xxx */
                {
                    if (len - (p - str) < 4) {
                        goto error;
                    }

                    min = (1 << 16);
                    val = *(uint8*)p & 0x07;
                } else {
                    goto error;
                }

                p++;
                NEXT_CHAR;
            two_remain:
                p++;
                NEXT_CHAR;
                p++;
                NEXT_CHAR;

                if ((val < min) || (!IS_VALID_UNICODE(val))) {
                    goto error;
                }
            }

            continue;
        error:
            p = last;
            break;
        }
    }

    if (len >= 0 && p != str + len) {
        return FALSE;
    } else {
        return TRUE;
    }
}

MP4Err MP4ConvertUTF16BEtoUTF8(const uint16** sourceStart, const uint16* sourceEnd,
                               uint8** targetStart, uint8* targetEnd) {
    MP4Err err = MP4NoErr;
    const uint16* source = *sourceStart;
    uint8* target = *targetStart;
    while (source < sourceEnd) {
        uint32 ch;
        unsigned short bytesToWrite = 0;
        const uint32 byteMask = 0xBF;
        const uint32 byteMark = 0x80;
        const uint16* oldSource =
                source; /* In case we have to back up because of target overflow. */

        /*
        ** Step 1 : decode UTF-16BE to ISO 10646 character (refer to RFC 2781)
        */
        ch = *source++;
        /* If we have a surrogate pair, convert to UTF32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                uint32 ch2 = *source;
                /* If it's a low surrogate, convert to UTF32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << HALF_SHIFT) + (ch2 - UNI_SUR_LOW_START) +
                         HALF_BASE;
                    ++source;
                } else {
                    /* it's an unpaired high surrogate */
                    --source; /* return to the illegal value itself */
                    err = MP4BadDataErr;
                    break;
                }
            } else {
                /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                err = MP4BadDataErr;
                break;
            }
        } else {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                err = MP4BadDataErr;
                break;
            }
        }

        /*
        ** Step 2 : encode ISO 10646 character to UTF-8 (refer to RFC 3629)
        */
        /* Figure out how many bytes the result will require */
        if (ch < (uint32)0x80) {
            bytesToWrite = 1;
        } else if (ch < (uint32)0x800) {
            bytesToWrite = 2;
        } else if (ch < (uint32)0x10000) {
            bytesToWrite = 3;
        } else if (ch < (uint32)0x110000) {
            bytesToWrite = 4;
        } else {
            bytesToWrite = 3;
            ch = UNI_REPLACEMENT_CHAR;
        }

        target += bytesToWrite;
        if (target > targetEnd) {
            source = oldSource; /* Back up source pointer! */
            target -= bytesToWrite;
            err = MP4BadDataErr;
            break;
        }

        switch (bytesToWrite) { /* note: everything falls through. */
            case 4:
                *--target = (uint8)((ch | byteMark) & byteMask);
                ch >>= 6;
                __attribute__((fallthrough));
            case 3:
                *--target = (uint8)((ch | byteMark) & byteMask);
                ch >>= 6;
                __attribute__((fallthrough));
            case 2:
                *--target = (uint8)((ch | byteMark) & byteMask);
                ch >>= 6;
                __attribute__((fallthrough));
            case 1:
                *--target = (uint8)(ch | g_firstByteMark[bytesToWrite]);
        }
        target += bytesToWrite;
    }

    *sourceStart = source;
    *targetStart = target;

    return err;
}

MP4Err MP4ConvertASCIItoUTF8(const uint8* sourceStart, uint32 sourceSize, uint8* targetStart,
                             uint32* targetSize) {
    MP4Err err = MP4NoErr;
    const uint8* source = sourceStart;
    const uint8* sourceEnd = sourceStart + sourceSize;
    uint8* target = targetStart;
    uint8* targetEnd = targetStart + *targetSize;

    while (source < sourceEnd) {
        if (*source > 0x7F) {
            *target = ((*source >> 6) & 0x1F) | 0xC0; /* 110x xxxx */
            target++;
            *target = (*source & 0x3F) | 0x80; /* 10xx xxxx */
        } else {
            *target = *source;
        }
        source++;
        target++;

        if (target > targetEnd) {
            err = PARSER_ERR_UNKNOWN;
            break;
        }
    }

    *targetSize = (uint32)(target - targetStart);

    return err;
}
