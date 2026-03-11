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
    MP4TrackExtendsAtomPtr self = (MP4TrackExtendsAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(track_ID);
    GET32(default_sample_description_index);
    GET32(default_sample_duration);
    GET32(default_sample_size);
    GET32(default_sample_flags);
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackExtendsAtom(MP4TrackExtendsAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackExtendsAtomPtr self;

    self = (MP4TrackExtendsAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackExtendsAtom));
    TESTMALLOC(self)
    err = MP4CreateFullAtom((MP4AtomPtr)self);
    self->type = MP4TrackExtendsAtomType;
    self->name = "track extends";
    self->track_ID = 0;
    self->default_sample_description_index = 0;
    self->default_sample_duration = 0;
    self->default_sample_size = 0;
    self->default_sample_flags = 0;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
