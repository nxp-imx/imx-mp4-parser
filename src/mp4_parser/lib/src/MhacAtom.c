/*
 ***********************************************************************
 * Copyright 2021, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MhacAtomPtr self;
    err = MP4NoErr;
    self = (MhacAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->csd) {
        MP4LocalFree(self->csd);
        self->csd = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 left = 0;
    u32 mhac_header_size = 5;

    MhacAtomPtr self = (MhacAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    left = self->size - self->bytesRead;
    if (left < mhac_header_size)
        BAILWITHERROR(MP4InvalidMediaErr)

    GET8(configVersion);
    GET8(profileLevelIndication);
    GET8(referenceChannelLayout);
    GET16(csdSize);

    left = self->size - self->bytesRead;
    if (left < self->csdSize)
        BAILWITHERROR(MP4InvalidMediaErr)

    self->csd = (u8*)MP4LocalCalloc(self->csdSize, sizeof(u8));
    TESTMALLOC(self->csd)
    GETBYTES(self->csdSize, csd);

    if (self->size > self->bytesRead) {
        u64 skipped_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(skipped_size);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMhacAtom(MhacAtomPtr* outAtom) {
    MP4Err err;
    MhacAtomPtr self;

    self = (MhacAtomPtr)MP4LocalCalloc(1, sizeof(MhacAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MHACAtomType;
    self->name = "MPEGH audio mhac";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->csdSize = 0;
    self->csd = NULL;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
