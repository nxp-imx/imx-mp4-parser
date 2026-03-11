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
    MP4MovieExtendsHeaderAtomPtr self = (MP4MovieExtendsHeaderAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->version == 1) {
        GET64(fragment_duration);
    } else {
        u32 val;
        GET32_V_MSG(val, "fragment_duration");
        self->fragment_duration = val;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMovieExtendsHeaderAtom(MP4MovieExtendsHeaderAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieExtendsHeaderAtomPtr self;

    self = (MP4MovieExtendsHeaderAtomPtr)MP4LocalCalloc(1, sizeof(MP4MovieExtendsHeaderAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4MovieExtendsHeaderAtomType;
    self->name = "movie extends header";

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->fragment_duration = 0;
    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
