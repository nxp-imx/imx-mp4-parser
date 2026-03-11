/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4TrackFragmentRandomAccessAtomPtr self;

    err = MP4NoErr;

    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)

    self = (MP4TrackFragmentRandomAccessAtomPtr)s;
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

static MP4Err seek(struct MP4TrackFragmentRandomAccessAtom* self, u32 flags, uint64* targetTime,
                   u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationTime;
    u64 outTime = 0;
    u64 outOffset = 0;
    u32 i;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationTime = *targetTime;

    if (self->version == 1) {
        u64* sample = (u64*)self->sample;

        if (SEEK_FLAG_NO_EARLIER == flags) {
            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] >= destinationTime) {
                    outTime = sample[i];
                    outOffset = sample[i + 1];
                    break;
                }
            }
            if (outTime < destinationTime)
                err = MP4EOF;
        }

        if (SEEK_FLAG_NO_LATER == flags) {
            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] > destinationTime) {
                    break;
                }
                outTime = sample[i];
                outOffset = sample[i + 1];
            }
        }

        if (SEEK_FLAG_NEAREST == flags) {
            u64 diff1;
            u64 diff2;
            u64 sampleTime = 0;
            u64 sampleOffset = 0;
            u64 previousSampleTime = 0;
            u64 previousSampleOffset = 0;

            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] >= destinationTime) {
                    sampleTime = sample[i];
                    sampleOffset = sample[i + 1];
                    if (i >= 2) {
                        previousSampleTime = sample[i - 2];
                        previousSampleOffset = sample[i - 1];
                    }
                    break;
                }
            }

            diff1 = sampleTime - destinationTime;
            diff2 = destinationTime - previousSampleTime;

            if (diff1 < diff2) {
                outTime = sampleTime;
                outOffset = sampleOffset;
            } else {
                outTime = previousSampleTime;
                outOffset = previousSampleOffset;
            }
        }

    } else {
        u32* sample = (u32*)self->sample;

        if (SEEK_FLAG_NO_EARLIER == flags) {
            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] >= destinationTime) {
                    outTime = sample[i];
                    outOffset = sample[i + 1];
                    break;
                }
            }
            if (outTime < destinationTime)
                err = MP4EOF;
        }

        if (SEEK_FLAG_NO_LATER == flags) {
            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] > destinationTime) {
                    break;
                }
                outTime = sample[i];
                outOffset = sample[i + 1];
            }
        }

        if (SEEK_FLAG_NEAREST == flags) {
            u64 diff1;
            u64 diff2;
            u64 sampleTime = 0;
            u64 sampleOffset = 0;
            u64 previousSampleTime = 0;
            u64 previousSampleOffset = 0;

            for (i = 0; i < self->sample_count * 2; i = i + 2) {
                if (sample[i] >= destinationTime) {
                    sampleTime = sample[i];
                    sampleOffset = sample[i + 1];
                    if (i >= 2) {
                        previousSampleTime = sample[i - 2];
                        previousSampleOffset = sample[i - 1];
                    }
                    break;
                }
            }

            diff1 = sampleTime - destinationTime;
            diff2 = destinationTime - previousSampleTime;

            if (diff1 < diff2) {
                outTime = sampleTime;
                outOffset = sampleOffset;
            } else {
                outTime = previousSampleTime;
                outOffset = previousSampleOffset;
            }
        }
    }
    *targetTime = outTime;
    *offset = outOffset;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err nextIndex(struct MP4TrackFragmentRandomAccessAtom* self, uint64* targetTime,
                        u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationOffset;
    u64 outTime = 0;
    u64 outOffset = 0;
    u32 i;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationOffset = *offset;

    if (self->version == 1) {
        u64* sample = (u64*)self->sample;

        if (destinationOffset == sample[self->sample_count * 2 - 1]) {
            BAILWITHERROR(PARSER_EOS);
        }

        for (i = 0; i < self->sample_count * 2; i = i + 2) {
            if (sample[i + 1] > destinationOffset) {
                outTime = sample[i];
                outOffset = sample[i + 1];
                break;
            }
        }

    } else {
        u32* sample = (u32*)self->sample;

        if (destinationOffset == (u64)sample[self->sample_count * 2 - 1]) {
            BAILWITHERROR(PARSER_EOS);
        }

        for (i = 0; i < self->sample_count * 2; i = i + 2) {
            if ((u64)sample[i + 1] > destinationOffset) {
                outTime = sample[i];
                outOffset = sample[i + 1];
                break;
            }
        }
    }
    *targetTime = outTime;
    *offset = outOffset;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err previousIndex(struct MP4TrackFragmentRandomAccessAtom* self, uint64* targetTime,
                            u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationOffset;
    u64 outTime = 0;
    u64 outOffset = 0;
    u32 i;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationOffset = *offset;

    if (self->version == 1) {
        u64* sample = (u64*)self->sample;

        if (destinationOffset == sample[1]) {
            BAILWITHERROR(PARSER_BOS);
        }

        for (i = 0; i < self->sample_count * 2; i = i + 2) {
            if (sample[i + 1] >= destinationOffset) {
                break;
            }
            outTime = sample[i];
            outOffset = sample[i + 1];
        }

    } else {
        u32* sample = (u32*)self->sample;

        if (destinationOffset == (u64)sample[1]) {
            BAILWITHERROR(PARSER_BOS);
        }

        for (i = 0; i < self->sample_count * 2; i = i + 2) {
            if ((u64)sample[i + 1] >= destinationOffset) {
                break;
            }
            outTime = sample[i];
            outOffset = sample[i + 1];
        }
    }
    *targetTime = outTime;
    *offset = outOffset;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4TrackFragmentRandomAccessAtomPtr self = (MP4TrackFragmentRandomAccessAtomPtr)s;
    u32 val;
    u32 i;
    u32 entry_size;
    u32 traf_num = 0;
    u32 trun_num = 0;
    u32 sample_num = 0;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(id);
    GET32_V_MSG(val, "size");
    GET32(number_of_entry);
    self->sample_size = (val & 0x03) + 1;
    self->trun_size = (val & 0x0C) + 1;
    self->traf_size = (val & 0x30) + 1;
    self->sample_count = 0;

    if (1 == self->version) {
        entry_size = sizeof(u64) * 2;
    } else {
        entry_size = sizeof(u32) * 2;
    }
    entry_size += self->sample_size + self->trun_size + self->traf_size;

    if (entry_size * self->number_of_entry + self->bytesRead != self->size)
        BAILWITHERROR(MP4BadDataErr)

    if (1 == self->version) {
        u64 time = 0;
        u64 offset = 0;

        u64* samplePtr = (u64*)MP4LocalCalloc(self->number_of_entry, sizeof(u64) * 2);
        TESTMALLOC(samplePtr);

        for (i = 0; i < self->number_of_entry * 2; i = i + 2) {
            GET64_V_MSG(time, "time");
            GET64_V_MSG(offset, "time");
            GETBYTES_V_MSG(self->traf_size, &traf_num, "traf num");
            GETBYTES_V_MSG(self->trun_size, &trun_num, "trun num");
            GETBYTES_V_MSG(self->sample_size, &sample_num, "sample num");
            if (1 == traf_num && 1 == trun_num && 1 == sample_num) {
                samplePtr[i] = time;
                samplePtr[i + 1] = offset;
                self->sample_count++;
            }
        }
        self->last_ts = time;
        self->last_offset = offset;
        self->sample = samplePtr;

    } else {
        u32 time;
        u32 offset;

        u32* samplePtr = (u32*)MP4LocalCalloc(self->number_of_entry, sizeof(u32) * 2);
        TESTMALLOC(samplePtr);

        for (i = 0; i < self->number_of_entry * 2; i = i + 2) {
            GET32_V_MSG(time, "time");
            GET32_V_MSG(offset, "time");
            GETBYTES_V_MSG(self->traf_size, &traf_num, "traf num");
            GETBYTES_V_MSG(self->trun_size, &trun_num, "trun num");
            GETBYTES_V_MSG(self->sample_size, &sample_num, "sample num");

            // only use the first sample start from moof, drop others.
            if (1 == traf_num && 1 == trun_num && 1 == sample_num) {
                samplePtr[i] = time;
                samplePtr[i + 1] = offset;
                self->sample_count++;
            }
        }

        self->last_ts = time;
        self->last_offset = offset;
        self->sample = samplePtr;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackFragmentRandomAccessAtom(MP4TrackFragmentRandomAccessAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackFragmentRandomAccessAtomPtr self;

    self = (MP4TrackFragmentRandomAccessAtomPtr)MP4LocalCalloc(
            1, sizeof(MP4TrackFragmentRandomAccessAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4TrackFragmentRandomAccessAtomType;
    self->name = "track fragment random access";

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->number_of_entry = 0;
    self->id = 0;
    self->sample_size = 0;
    self->trun_size = 0;
    self->traf_size = 0;
    self->sample_count = 0;
    self->seek = seek;
    self->nextIndex = nextIndex;
    self->previousIndex = previousIndex;
    self->last_ts = 0;
    self->last_offset = 0;
    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
