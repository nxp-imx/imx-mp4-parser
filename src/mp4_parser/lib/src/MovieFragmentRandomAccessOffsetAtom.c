/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;

    err = MP4NoErr;

    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (s->super)
        s->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4MovieFragmentRandomAccessOffsetAtomPtr self = (MP4MovieFragmentRandomAccessOffsetAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(mfra_size);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMovieFragmentRandomAccessOffsetAtom(
        MP4MovieFragmentRandomAccessOffsetAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieFragmentRandomAccessOffsetAtomPtr self;

    self = (MP4MovieFragmentRandomAccessOffsetAtomPtr)MP4LocalCalloc(
            1, sizeof(MP4MovieFragmentRandomAccessOffsetAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4MovieFragmentRandomAccessOffsetAtomType;
    self->name = "movie fragment random offset";

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->mfra_size = 0;
    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
