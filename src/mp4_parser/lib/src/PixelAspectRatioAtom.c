
/*
 * Copyright 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4PASPAtomPtr self;
    err = MP4NoErr;
    self = (MP4PASPAtomPtr)s;
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
    MP4PASPAtomPtr self = (MP4PASPAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(hSpacing);
    GET32(vSpacing);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreatePixelAspectRatioAtom(MP4PASPAtomPtr* outAtom) {
    MP4Err err;
    MP4PASPAtomPtr self;

    self = (MP4PASPAtomPtr)MP4LocalCalloc(1, sizeof(MP4PASPAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4PixelAspectRatioAtomType;
    self->name = "pasp atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
