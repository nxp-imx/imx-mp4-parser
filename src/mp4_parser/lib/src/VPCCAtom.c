
/*
 * Copyright 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4VPCCAtomPtr self;
    self = (MP4VPCCAtomPtr)s;

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
    MP4VPCCAtomPtr self = (MP4VPCCAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    bytesToRead = (u32)(s->size - s->bytesRead);

    if (bytesToRead > 0) {
        self->data = (u8*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)
        GETBYTES_MSG(bytesToRead, data, "VPCC");
        self->dataSize = bytesToRead;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateVpccAtom(MP4VPCCAtomPtr* outAtom) {
    MP4Err err;
    MP4VPCCAtomPtr self;

    self = (MP4VPCCAtomPtr)MP4LocalCalloc(1, sizeof(MP4VPCCAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->name = "VP9";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    self->dataSize = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
