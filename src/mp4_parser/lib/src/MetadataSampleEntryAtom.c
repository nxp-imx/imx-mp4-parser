/*
 ***********************************************************************
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MetadataSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MetadataSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MetadataSampleEntryAtomPtr self = (MetadataSampleEntryAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;
    self->dataReferenceIndex = 1;
    self->dataSize = (u32)(self->size - self->bytesRead);
    if (self->dataSize > 0) {
        self->data = (char*)MP4LocalMalloc(self->dataSize);
        TESTMALLOC(self->data);
        GETBYTES(self->dataSize, data);
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMetadataSampleEntryAtom(MetadataSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MetadataSampleEntryAtomPtr self;

    self = (MetadataSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MetadataSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MetadataSampleEntryAtomType;
    self->name = "timed text metadata entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->dataReferenceIndex = 1;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
