/*
 ***********************************************************************
 * Copyright 2021, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MhapAtomPtr self;
    err = MP4NoErr;
    self = (MhapAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->compatibleSets) {
        MP4LocalFree(self->compatibleSets);
        self->compatibleSets = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    int left = 0;

    MhapAtomPtr self = (MhapAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    left = self->size - self->bytesRead;
    if (left < 1)
        BAILWITHERROR(MP4InvalidMediaErr)

    GET8(compatibleSetsSize);
    if (self->compatibleSetsSize > self->size - self->bytesRead)
        BAILWITHERROR(MP4InvalidMediaErr)

    self->compatibleSets = (u8*)MP4LocalCalloc(self->compatibleSetsSize, sizeof(u8));
    TESTMALLOC(self->compatibleSets)

    GETBYTES(self->compatibleSetsSize, compatibleSets);

    if (self->size > self->bytesRead) {
        u64 skipped_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(skipped_size);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMhapAtom(MhapAtomPtr* outAtom) {
    MP4Err err;
    MhapAtomPtr self;

    self = (MhapAtomPtr)MP4LocalCalloc(1, sizeof(MhapAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MHAPAtomType;
    self->name = "MPEGH audio mhap";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->compatibleSets = NULL;
    self->compatibleSetsSize = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
