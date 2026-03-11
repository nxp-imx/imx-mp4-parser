/*
 ***********************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4OriginFormatAtomPtr self;
    err = MP4NoErr;
    self = (MP4OriginFormatAtomPtr)s;
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
    MP4OriginFormatAtomPtr self = (MP4OriginFormatAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(format);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateOriginFormatAtom(MP4OriginFormatAtomPtr* outAtom) {
    MP4Err err;
    MP4OriginFormatAtomPtr self;

    self = (MP4OriginFormatAtomPtr)MP4LocalCalloc(1, sizeof(MP4OriginFormatAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4OriginFormatAtomType;
    self->name = "Origin Format Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}