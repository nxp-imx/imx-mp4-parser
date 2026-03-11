/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    PrimaryItemAtomPtr self;
    err = MP4NoErr;
    self = (PrimaryItemAtomPtr)s;
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
    int itemIdSize;

    PrimaryItemAtomPtr self = (PrimaryItemAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    itemIdSize = self->version == 0 ? 2 : 4;

    if (itemIdSize == 2) {
        GET16(itemId);
    } else {
        GET32(itemId);
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreatePrimaryItemAtom(PrimaryItemAtomPtr* outAtom) {
    MP4Err err;
    PrimaryItemAtomPtr self;

    self = (PrimaryItemAtomPtr)MP4LocalCalloc(1, sizeof(PrimaryItemAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = PrimaryItemAtomType;
    self->name = "Primary Item Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->itemId = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
