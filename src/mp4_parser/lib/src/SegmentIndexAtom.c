/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SegmentIndexAtomPtr self;
    err = MP4NoErr;
    self = (MP4SegmentIndexAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->sampleNumbers) {
        MP4LocalFree(self->sampleNumbers);
        self->sampleNumbers = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err seek(struct MP4SegmentIndexAtom* self, u32 flags, uint64* targetTime, u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationTime;
    u32 outTime = 0;
    u32 outOffset = 0;
    u32 i;
    u32 sampleTime = 0;
    u32 sampleOffset = 0;
    u32 previousSampleTime = 0;
    u32 previousSampleOffset = 0;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationTime = *targetTime;

    if (SEEK_FLAG_NO_EARLIER == flags) {
        for (i = 0; i < self->entryCount * 2; i = i + 2) {
            if (self->sampleNumbers[i] >= destinationTime) {
                outTime = self->sampleNumbers[i];
                outOffset = self->sampleNumbers[i + 1];
                break;
            }
        }
    }

    if (SEEK_FLAG_NO_LATER == flags) {
        for (i = 0; i < self->entryCount * 2; i = i + 2) {
            if (self->sampleNumbers[i] > destinationTime) {
                break;
            }
            outTime = self->sampleNumbers[i];
            outOffset = self->sampleNumbers[i + 1];
        }
    }

    if (SEEK_FLAG_NEAREST == flags) {
        u64 diff1;
        u64 diff2;
        for (i = 0; i < self->entryCount * 2; i = i + 2) {
            if (self->sampleNumbers[i] >= destinationTime) {
                sampleTime = self->sampleNumbers[i];
                sampleOffset = self->sampleNumbers[i + 1];
                if (i >= 2) {
                    previousSampleTime = self->sampleNumbers[i - 2];
                    previousSampleOffset = self->sampleNumbers[i - 1];
                }
                break;
            }
        }

        diff1 = (u64)sampleTime - destinationTime;
        diff2 = destinationTime - previousSampleTime;

        if (diff1 < diff2) {
            outTime = sampleTime;
            outOffset = sampleOffset;
        } else {
            outTime = previousSampleTime;
            outOffset = previousSampleOffset;
        }
    }

    *targetTime = (u64)outTime;
    *offset = (u64)outOffset;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err nextIndex(struct MP4SegmentIndexAtom* self, uint64* targetTime, u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationOffset;
    u32 outTime = 0;
    u32 outOffset = 0;
    u32 i;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationOffset = *offset;

    if (destinationOffset == self->sampleNumbers[self->entryCount * 2 - 1]) {
        BAILWITHERROR(PARSER_EOS);
    }

    for (i = 0; i < self->entryCount * 2; i = i + 2) {
        if (self->sampleNumbers[i + 1] > destinationOffset) {
            outTime = self->sampleNumbers[i];
            outOffset = self->sampleNumbers[i + 1];
            break;
        }
    }

    *targetTime = (u64)outTime;
    *offset = (u64)outOffset;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err previousIndex(struct MP4SegmentIndexAtom* self, uint64* targetTime, u64* offset) {
    MP4Err err = MP4NoErr;
    u64 destinationOffset;
    u32 outTime = 0;
    u32 outOffset = 0;
    u32 i;

    if (targetTime == NULL || self == NULL || offset == NULL)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);

    destinationOffset = *offset;

    if (destinationOffset == 0) {
        BAILWITHERROR(PARSER_BOS);
    }

    for (i = 0; i < self->entryCount * 2; i = i + 2) {
        if (self->sampleNumbers[i + 1] >= destinationOffset) {
            break;
        }
        outTime = self->sampleNumbers[i];
        outOffset = self->sampleNumbers[i + 1];
    }

    *targetTime = (u64)outTime;
    *offset = (u64)outOffset;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4SegmentIndexAtomPtr self = (MP4SegmentIndexAtomPtr)s;

    u32 i, j;
    u32 val = 0;
    u32 timescale;
    u32 entry_cnt;
    u64 current_ts = 0;
    u32 offset = 0;
    u32 reserved1;
    u64 duration = 0;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(id);
    GET32_V_MSG(timescale, "timescale");

    if (self->version == 0) {
        GET32_V_MSG(val, "earliest_presentation_time");
        GET32_V_MSG(val, "first_offset");
    } else {
        u64 val_64;
        GET64_V_MSG(val_64, "earliest_presentation_time");
        GET64_V_MSG(val_64, "first_offset");
        (void)val_64;
    }

    GET16_V_MSG(reserved1, "reserved");
    GET16(entryCount);

    entry_cnt = self->entryCount;
    if (self->bytesRead + 3 * sizeof(u32) * entry_cnt > self->size)
        BAILWITHERROR(MP4BadParamErr)

    self->sampleNumbers = (u32*)MP4LocalCalloc(entry_cnt, (sizeof(u32) + sizeof(u32)));
    TESTMALLOC(self->sampleNumbers);

    for (i = 0, j = 0; i < entry_cnt; i++) {
        GET32_V_MSG(val, "size");
        offset += (val & 0x7fffffff);
        GET32_V_MSG(val, "duration");
        current_ts += val;
        duration += val;
        GET32_V_MSG(val, "flag");
        self->sampleNumbers[j] = (u32)(current_ts * 1000000 / timescale);
        self->sampleNumbers[j + 1] = offset;
        j += 2;
        MP4MSG("segment index offset=%d,ts=%lld \n", offset, (current_ts * 1000000 / timescale));
    }
    self->duration = duration * 1000000 / timescale;

bail:
    TEST_RETURN(err);

    if (err) {
        if (self && self->sampleNumbers) {
            MP4LocalFree(self->sampleNumbers);
            self->sampleNumbers = NULL;
        }
    }

    return err;
}

MP4Err MP4CreateSegmentIndexAtom(MP4SegmentIndexAtomPtr* outAtom) {
    MP4Err err;
    MP4SegmentIndexAtomPtr self;

    self = (MP4SegmentIndexAtomPtr)MP4LocalCalloc(1, sizeof(MP4SegmentIndexAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SegmentIndexAtomType;
    self->name = "segment index";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->seek = seek;
    self->nextIndex = nextIndex;
    self->previousIndex = previousIndex;
    self->duration = 0;
    // TODO: according to iso/iec 14496-12 8.16.3, count of sidx box maybe more than one.
    // we need to merge other sidx box to this one.

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
