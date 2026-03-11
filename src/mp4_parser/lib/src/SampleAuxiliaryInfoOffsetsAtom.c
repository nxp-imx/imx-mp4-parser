/*
 ***********************************************************************
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

#define SAIO_TAB_MAX_SIZE 100000

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleAuxiliaryInfoOffsetsAtomPtr self;
    err = MP4NoErr;
    self = (MP4SampleAuxiliaryInfoOffsetsAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->offsets_u32) {
        MP4LocalFree(self->offsets_u32);
        self->offsets_u32 = NULL;
    }
    if (self->offsets_u64) {
        MP4LocalFree(self->offsets_u64);
        self->offsets_u64 = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getSampleAuxiliaryInfoOffsets(MP4SampleAuxiliaryInfoOffsetsAtomPtr s, u32 entryNumber,
                                            u64* outOffset) {
    MP4Err err;
    MP4SampleAuxiliaryInfoOffsetsAtomPtr self = (MP4SampleAuxiliaryInfoOffsetsAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outOffset == NULL) || (entryNumber > self->entry_count))
        BAILWITHERROR(MP4BadParamErr)

    if (0 == self->entry_count)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    if (self->version == 0)
        *outOffset = (u64)self->offsets_u32[entryNumber];
    else
        *outOffset = self->offsets_u64[entryNumber];

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4SampleAuxiliaryInfoOffsetsAtomPtr self = (MP4SampleAuxiliaryInfoOffsetsAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->flags & 1) {
        GET32(aux_info_type);
        GET32(aux_info_type_parameter);
    }
    GET32(entry_count);

    if (self->entry_count > SAIO_TAB_MAX_SIZE)
        BAILWITHERROR(MP4BadParamErr)

    if (self->entry_count == 0) {
        if (self->size > self->bytesRead) {
            u64 bytesToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(bytesToSkip);
        }
        goto bail;
    }

    if (self->version == 0) {
        self->offsets_u32 = (u32*)MP4LocalCalloc(self->entry_count, sizeof(u32));
        TESTMALLOC(self->offsets_u32)

        GETBYTES(self->entry_count * sizeof(u32), offsets_u32);

#ifndef CPU_BIG_ENDIAN
        reverse_endian_u32(self->offsets_u32, self->entry_count);
#endif

    } else {
        self->offsets_u64 = (u64*)MP4LocalCalloc(self->entry_count, sizeof(u64));
        TESTMALLOC(self->offsets_u64)

        GETBYTES(self->entry_count * sizeof(u64), offsets_u64);

#ifndef CPU_BIG_ENDIAN
        reverse_endian_u64(self->offsets_u64, self->entry_count);
#endif
    }

    if (self->size > self->bytesRead) {
        u64 bytesToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);

    if (err) {
        if (self->offsets_u32) {
            MP4LocalFree(self->offsets_u32);
            self->offsets_u32 = NULL;
        }
        if (self->offsets_u64) {
            MP4LocalFree(self->offsets_u64);
            self->offsets_u64 = NULL;
        }
    }

    return err;
}

MP4Err MP4CreateSampleAuxiliaryInfoOffsetsAtom(MP4SampleAuxiliaryInfoOffsetsAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleAuxiliaryInfoOffsetsAtomPtr self;

    self = (MP4SampleAuxiliaryInfoOffsetsAtomPtr)MP4LocalCalloc(
            1, sizeof(MP4SampleAuxiliaryInfoOffsetsAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SampleAuxiliaryInfoOffsetAtomType;
    self->name = "sample auxiliary info offsets";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getSampleOffsets = getSampleAuxiliaryInfoOffsets;
    self->aux_info_type = 0;
    self->aux_info_type_parameter = 0;
    self->entry_count = 0;
    self->offsets_u32 = NULL;
    self->offsets_u64 = NULL;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
