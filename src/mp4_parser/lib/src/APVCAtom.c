/*
 * Copyright 2025-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4APVCAtomPtr self;
    self = (MP4APVCAtomPtr)s;

    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    u32 bytesToRead;
    MP4APVCAtomPtr self = (MP4APVCAtomPtr)s;
    u32 skipBytes = 4;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    bytesToRead = (u32)(s->size - s->bytesRead);
    if (bytesToRead > skipBytes) {
        SKIPBYTES_FORWARD(skipBytes);
        bytesToRead -= skipBytes;
    }

    if (bytesToRead > 0) {
        self->data = (u8*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)
        GETBYTES_MSG(bytesToRead, data, "APVC");
        self->dataSize = bytesToRead;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateApvcAtom(MP4APVCAtomPtr* outAtom) {
    MP4Err err;
    MP4APVCAtomPtr self;

    self = (MP4APVCAtomPtr)MP4LocalCalloc(1, sizeof(MP4APVCAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->name = "APVC";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    self->dataSize = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
