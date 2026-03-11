
/*
 * Copyright 2020, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4BitrateAtomPtr self;
    err = MP4NoErr;
    self = (MP4BitrateAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4BitrateAtomPtr self = (MP4BitrateAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(4, reserved1);
    GET32(maxBitrate);
    GET32(avgBitrate);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateBitrateAtom(MP4BitrateAtomPtr* outAtom) {
    MP4Err err;
    MP4BitrateAtomPtr self;

    self = (MP4BitrateAtomPtr)MP4LocalCalloc(1, sizeof(MP4BitrateAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4BitrateAtomType;
    self->name = "bitrate atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
