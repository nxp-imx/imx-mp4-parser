/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2024, 2026 NXP
 ***********************************************************************
 */

/*
This software module was originally developed by Apple Computer, Inc.
in the course of development of MPEG-4.
This software module is an implementation of a part of one or
more MPEG-4 tools as specified by MPEG-4.
ISO/IEC gives users of MPEG-4 free license to this
software module or modifications thereof for use in hardware
or software products claiming conformance to MPEG-4.
Those intending to use this software module in hardware or software
products are advised that its use may infringe existing patents.
The original developer of this software module and his/her company,
the subsequent editors and their companies, and ISO/IEC have no
liability for use of this software module or modifications thereof
in an implementation.
Copyright is not released for non MPEG-4 conforming
products. Apple Computer, Inc. retains full right to use the code for its own
purpose, assign or donate the code to a third party and to
inhibit third parties from using the code for non
MPEG-4 conforming products.
This copyright notice must be included in all copies or
derivative works. Copyright (c) 1999.
*/
/*
    $Id: ValueAtom.c,v 1.2 2000/05/17 08:01:32 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "Charset.h"
#include "MP4Atoms.h"

#define MAX_VALUE_SIZE_FOR_STRING 1024

static void destroy(MP4AtomPtr s) {
    MP4ValueAtomPtr self;
    self = (MP4ValueAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

__attribute__((unused)) static MP4Err calculateSize(struct MP4Atom* s) {
    MP4Err err = MP4NoErr;
    MP4ValueAtomPtr self = (MP4ValueAtomPtr)s;

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
    MP4ValueAtomPtr self = (MP4ValueAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    GET32(valueType);
    if ((self->valueType & 0xFF000000) != 0)
        BAILWITHERROR(MP4BadParamErr)
    GET16(countryCode);
    GET16(languageCode);

    bytesToRead = (long)(s->size - s->bytesRead);
    if (bytesToRead > 0) {
        // size protection for CT 41748297
        if ((self->valueType > 0 && self->valueType <= QT_WELL_KNOWN_DATA_TYPE_UTF_16BE) &&
            (bytesToRead > MAX_VALUE_SIZE_FOR_STRING)) {
            bytesToRead = MAX_VALUE_SIZE_FOR_STRING;
        }
        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)

        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        /* if the data type is UTF-16BE, convert it to UTF-8 */
        if (QT_WELL_KNOWN_DATA_TYPE_UTF_16BE == self->valueType) {
            int32 ret = MP4NoErr;
            const uint16 *srcStart, *srcEnd;
            uint8 *dstStart, *dstEnd;
            uint32 dstSize = bytesToRead << 1;
            void* src = self->data;
            void* dst = (char*)MP4LocalCalloc(1, dstSize);
            TESTMALLOC(dst)

            srcStart = (uint16*)src;
            srcEnd = (uint16*)((uint8*)src + bytesToRead);
            dstStart = (uint8*)dst;
            dstEnd = (uint8*)dst + dstSize;
            ret = MP4ConvertUTF16BEtoUTF8(&srcStart, srcEnd, &dstStart, dstEnd);
            if (MP4NoErr == ret) {
                MP4LocalFree(self->data);

                self->data = dst;
                self->dataSize = (uint8*)dstStart - (uint8*)dst;
                self->valueType = QT_WELL_KNOWN_DATA_TYPE_UTF_8;
            } else {
                MP4LocalFree(dst);
            }
        }

        if (QT_WELL_KNOWN_DATA_TYPE_UTF_8 == self->valueType) {
            if (MP4StringisUTF8(self->data, self->dataSize)) {
                MP4MSG("Got a UTF-8 string\n");
                goto bail;
            }

            /* FIXME: is no UTF-8 string. convert it to UTF-8 */
            MP4MSG("WARNING: The string is not UTF-8. Needs conversion\n");
        }
    }

bail:
    TEST_RETURN(err);

    if (err && self != NULL && self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    return err;
}

MP4Err MP4CreateValueAtom(MP4ValueAtomPtr* outAtom) {
    MP4Err err;
    MP4ValueAtomPtr self;

    self = (MP4ValueAtomPtr)MP4LocalCalloc(1, sizeof(MP4ValueAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = QTValueAtomType;
    self->name = "value atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    // self->calculateSize         = calculateSize;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
