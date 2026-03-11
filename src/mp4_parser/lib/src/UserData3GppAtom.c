/*
 ***********************************************************************
 * Copyright (c) 2015-2016, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "Charset.h"
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4UserData3GppAtomPtr self;
    self = (MP4UserData3GppAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

static MP4Err serialize(struct MP4Atom* s, char* buffer) {
    MP4Err err = MP4NoErr;
    MP4UserData3GppAtomPtr self = (MP4UserData3GppAtomPtr)s;

    err = MP4SerializeCommonBaseAtomFields(s, buffer);
    if (err)
        goto bail;
    buffer += self->bytesWritten;

    PUT16(stringSize);
    PUT16(languageCode);

    if (self->dataSize && self->data) {
        PUTBYTES(self->data, self->dataSize);
    }
    assert(self->bytesWritten == self->size);
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err calculateSize(struct MP4Atom* s) {
    MP4Err err = MP4NoErr;
    MP4UserData3GppAtomPtr self = (MP4UserData3GppAtomPtr)s;

    err = MP4CalculateBaseAtomFieldSize(s);
    if (err)
        goto bail;
    self->size += 4 + self->dataSize;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    long bytesToRead;
    MP4UserData3GppAtomPtr self = (MP4UserData3GppAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    if (self->type == Udta3GppYearType) {
        uint16 year;
        bytesToRead = (long)(s->size - s->bytesRead);
        if (bytesToRead < 6)
            goto bail;

        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)
        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        year = ((uint16)((uint8)self->data[4]) << 8) + (uint16)((uint8)self->data[5]);

        if (year > 1000 && year < 10000) {
            char* tmp = (char*)MP4LocalCalloc(1, 4);
            TESTMALLOC(tmp)
            sprintf(tmp, "%u", year);
            MP4LocalFree(self->data);
            self->data = tmp;
            self->dataSize = 4;
        }
        goto bail;
    }

    if (self->size > 14) {
        u64 bytesToSkip = 6;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

    bytesToRead = (long)(s->size - s->bytesRead);
    if (bytesToRead > 0) {
        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)

        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        /* check if the string is UTF-8 */
        if (MP4StringisUTF8(self->data, self->dataSize)) {
            MP4MSG("Got a UTF-8 string\n");
            goto bail;
        }

        if ((uint8)self->data[0] == 0xfe && (uint8)self->data[1] == 0xff) {
            uint32 i;
            char swap;
            for (i = 0; i < self->dataSize / 2; i += 2) {
                swap = self->data[i + 1];
                self->data[i + 1] = self->data[i];
                self->data[i] = swap;
            }
        }

        if ((uint8)self->data[0] == 0xff && (uint8)self->data[1] == 0xfe) {
            MP4Err ret;
            const uint16 *srcStart, *srcEnd;
            uint8 *dstStart, *dstEnd;
            uint32 dstSize = bytesToRead << 1;
            void* src = self->data;
            void* dst = (char*)MP4LocalCalloc(1, dstSize);
            TESTMALLOC(dst)

            srcStart = (uint16*)src + 1;
            srcEnd = (uint16*)((uint8*)src + bytesToRead);
            dstStart = (uint8*)dst;
            dstEnd = (uint8*)dst + dstSize;
            ret = MP4ConvertUTF16BEtoUTF8(&srcStart, srcEnd, &dstStart, dstEnd);
            if (MP4NoErr == ret) {
                MP4LocalFree(self->data);
                self->data = dst;
                self->dataSize = (uint8*)dstStart - (uint8*)dst;
            } else {
                MP4LocalFree(dst);
            }
            goto bail;
        }

        /* FIXME: is no UTF-8 string. we assume it is (extended) ascii */
        MP4MSG("WARNING: The string is not UTF-8. Assume it is extended ASCII\n");
        {
            char* asciiData = self->data;
            u32 asciiDataSize = self->dataSize;

            self->dataSize = self->dataSize * 2;
            self->data = (char*)MP4LocalCalloc(1, self->dataSize);
            TESTMALLOC(self->data)

            err = MP4ConvertASCIItoUTF8((uint8*)asciiData, asciiDataSize, (uint8*)self->data,
                                        &(self->dataSize));
        }
    }

bail:
    TEST_RETURN(err);

    if (err && self && self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    return err;
}

MP4Err MP4Create3GppUserDataAtom(MP4UserData3GppAtomPtr* outAtom) {
    MP4Err err;
    MP4UserData3GppAtomPtr self;

    self = (MP4UserData3GppAtomPtr)MP4LocalCalloc(1, sizeof(MP4UserData3GppAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->name = "3gpp user data atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    self->calculateSize = calculateSize;
    self->serialize = serialize;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
