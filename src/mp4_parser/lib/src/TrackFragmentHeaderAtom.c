/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#define FLAG_DATA_OFFSET 0x000001
#define FLAG_SAMPLE_DESCRIPTION_INDEX 0x000002
#define FLAG_SAMPLE_DURATION 0x000008
#define FLAG_SAMPLE_SIZE 0x000010
#define FLAG_SAMPLE_FLAG 0x000020
#define FLAG_EMPTY_DURATION 0x010000

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

static MP4Err getInfo(MP4TrackFragmentHeaderAtomPtr self, u64* base_data_offset,
                      u32* default_sample_duration, u32* default_sample_size,
                      u32* default_sample_flags) {
    MP4Err err = MP4NoErr;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr);

    if (!base_data_offset || !default_sample_duration || !default_sample_size ||
        !default_sample_flags)
        BAILWITHERROR(MP4BadParamErr);

    *base_data_offset = self->base_data_offset;
    *default_sample_duration = self->default_sample_duration;
    *default_sample_size = self->default_sample_size;
    *default_sample_flags = self->default_sample_flags;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4TrackFragmentHeaderAtomPtr self = (MP4TrackFragmentHeaderAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(track_ID);
    if (self->flags & FLAG_DATA_OFFSET) {
        GET64(base_data_offset);
    }
    if (self->flags & FLAG_SAMPLE_DESCRIPTION_INDEX) {
        GET32(sample_description_index);
    }
    if (self->flags & FLAG_SAMPLE_DURATION) {
        GET32(default_sample_duration);
    }
    if (self->flags & FLAG_SAMPLE_SIZE) {
        GET32(default_sample_size);
    }
    if (self->flags & FLAG_SAMPLE_FLAG) {
        GET32(default_sample_flags);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackFragmentHeaderAtom(MP4TrackFragmentHeaderAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackFragmentHeaderAtomPtr self;

    self = (MP4TrackFragmentHeaderAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackFragmentHeaderAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4TrackFragmentHeaderAtomType;
    self->name = "track fragment header";

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->track_ID = 0;

    self->base_data_offset = -1;
    self->sample_description_index = 0;
    self->default_sample_duration = 0;
    self->default_sample_size = 0;
    self->default_sample_flags = 0;
    self->getInfo = getInfo;

    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
