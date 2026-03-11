
/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    ItemDataAtomPtr self;
    self = (ItemDataAtomPtr)s;

    if (self->super)
        self->super->destroy(s);
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    ItemDataAtomPtr self = (ItemDataAtomPtr)s;

    err = MP4NoErr;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    self->offset = inputStream->getFilePos(inputStream, NULL);
    self->dataSize = s->size - s->bytesRead;

    SKIPBYTES_FORWARD(self->dataSize);

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemDataAtom(ItemDataAtomPtr* outAtom) {
    MP4Err err;
    ItemDataAtomPtr self;

    self = (ItemDataAtomPtr)MP4LocalCalloc(1, sizeof(ItemDataAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemDataAtomType;
    self->name = "item data atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->offset = 0;
    self->dataSize = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
