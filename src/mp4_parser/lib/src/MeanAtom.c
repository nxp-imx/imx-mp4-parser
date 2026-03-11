/*
 ***********************************************************************
 * Copyright (c) 2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "Charset.h"
#include "MP4Atoms.h"

#define MAX_VALUE_SIZE_FOR_STRING 1024

static void destroy(MP4AtomPtr s) {
    MP4MeanAtomPtr self;
    self = (MP4MeanAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    long bytesToRead;
    MP4MeanAtomPtr self = (MP4MeanAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    bytesToRead = (long)(s->size - s->bytesRead);
    if (bytesToRead > 0) {
        if (bytesToRead > MAX_VALUE_SIZE_FOR_STRING) {
            bytesToRead = MAX_VALUE_SIZE_FOR_STRING;
        }
        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)

        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        if (MP4StringisUTF8(self->data, self->dataSize)) {
            MP4MSG("Got a UTF-8 string\n");
            goto bail;
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

MP4Err MP4CreateMeanAtom(MP4MeanAtomPtr* outAtom) {
    MP4Err err;
    MP4MeanAtomPtr self;

    self = (MP4MeanAtomPtr)MP4LocalCalloc(1, sizeof(MP4NameAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4MeanAtomType;
    self->name = "mean atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    self->dataSize = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
