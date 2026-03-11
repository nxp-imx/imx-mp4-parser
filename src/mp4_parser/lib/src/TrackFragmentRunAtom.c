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
#define FLAG_FIRST_SAMPLE 0x000004
#define FLAG_SAMPLE_DURATION 0x000100
#define FLAG_SAMPLE_SIZE 0x000200
#define FLAG_SAMPLE_FLAG 0x000400
#define FLAG_COMPOSITION_TIME_OFFSET 0x000800
#define FLAG_EMPTY_DURATION 0x100000

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4TrackFragmentRunAtomPtr self;

    err = MP4NoErr;

    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)

    self = (MP4TrackFragmentRunAtomPtr)s;
    if (self->sample) {
        MP4LocalFree(self->sample);
        self->sample = NULL;
    }

    if (s->super)
        s->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4TrackFragmentRunAtomPtr self = (MP4TrackFragmentRunAtomPtr)s;
    u32 val;
    u32 sample_size = 0;
    u32 i;
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(sample_count);

    if (self->flags & FLAG_DATA_OFFSET) {
        GET32_V_MSG(val, "data_offset");
        self->data_offset = val;
    }

    if (self->flags & FLAG_FIRST_SAMPLE) {
        GET32_V_MSG(val, "first_sample_flag");
        self->first_sample_flags = val;
    }

    if (self->flags & FLAG_SAMPLE_DURATION)
        sample_size += 4;
    if (self->flags & FLAG_SAMPLE_SIZE)
        sample_size += 4;
    if (self->flags & FLAG_SAMPLE_FLAG)
        sample_size += 4;
    if (self->flags & FLAG_COMPOSITION_TIME_OFFSET)
        sample_size += 4;

    if (self->bytesRead + sample_size * self->sample_count != self->size)
        BAILWITHERROR(MP4BadParamErr)

    self->sample = (MP4TrackFragmentSample*)MP4LocalCalloc(self->sample_count,
                                                           sizeof(MP4TrackFragmentSample));
    TESTMALLOC(self->sample);

    for (i = 0; i < self->sample_count; i++) {
        if (self->flags & FLAG_SAMPLE_DURATION) {
            GET32_V_MSG(val, "duration");
            self->sample[i].duration = val;
        }

        if (self->flags & FLAG_SAMPLE_SIZE) {
            GET32_V_MSG(val, "size");
            self->sample[i].size = val;
        }

        if (self->flags & FLAG_SAMPLE_FLAG) {
            GET32_V_MSG(val, "flags");
            self->sample[i].flags = val;
        }

        if (self->flags & FLAG_COMPOSITION_TIME_OFFSET) {
            GET32_V_MSG(val, "ct offset");
            self->sample[i].compositionOffset = (int32)val;
        } else {
            self->sample[i].compositionOffset = 0;
        }
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackFragmentRunAtom(MP4TrackFragmentRunAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackFragmentRunAtomPtr self;

    self = (MP4TrackFragmentRunAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackFragmentRunAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4TrackFragmentRunAtomType;
    self->name = "track fragment run";

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->sample_size = 0;
    self->data_size = 0;
    self->first_sample_flags = 0;
    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
