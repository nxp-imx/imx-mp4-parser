/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define COLOR_TYPE_NCLC MP4_FOUR_CHAR_CODE('n', 'c', 'l', 'c')
#define COLOR_TYPE_NCLX MP4_FOUR_CHAR_CODE('n', 'c', 'l', 'x')

static void destroy(MP4AtomPtr s) {
    MP4Err err = MP4NoErr;
    QTColorParameterAtomPtr self = (QTColorParameterAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u64 bytesToSkip = 0;
    QTColorParameterAtomPtr self = (QTColorParameterAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    /* 10 bytes */
    GET32(colorParamType);  // do not check four cc type any more

    if (self->colorParamType == COLOR_TYPE_NCLX || self->colorParamType == COLOR_TYPE_NCLC) {
        GET16(primariesIndex);
        GET16(transferFuncIndex);
        GET16(matrixIndex);
    }

    if (self->colorParamType == COLOR_TYPE_NCLX) {
        GET8(full_range_flag);
    }

    bytesToSkip = s->size - s->bytesRead;
    if (bytesToSkip) {
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateQTColorParameterAtom(QTColorParameterAtomPtr* outAtom) {
    MP4Err err;
    QTColorParameterAtomPtr self;

    self = (QTColorParameterAtomPtr)MP4LocalCalloc(1, sizeof(QTColorParameterAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = QTColorParameterAtomType;
    self->name = "QT color parameter atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->full_range_flag = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
