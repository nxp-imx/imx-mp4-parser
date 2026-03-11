/*
 ***********************************************************************
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define SAIZ_TAB_MAX_SIZE 100000

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleAuxiliaryInfoSizeAtomPtr self;
    err = MP4NoErr;
    self = (MP4SampleAuxiliaryInfoSizeAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->sample_info_size) {
        MP4LocalFree(self->sample_info_size);
        self->sample_info_size = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4SampleAuxiliaryInfoSizeAtomPtr self = (MP4SampleAuxiliaryInfoSizeAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->flags & 1) {
        GET32(aux_info_type);
        GET32(aux_info_type_parameter);
    }
    GET8(default_sample_info_size);
    GET32(sample_count);

    if (self->sample_count > SAIZ_TAB_MAX_SIZE)
        BAILWITHERROR(MP4BadParamErr)

    if (self->default_sample_info_size == 0) {
        self->sample_info_size = (u8*)MP4LocalCalloc(self->sample_count, sizeof(u8));
        TESTMALLOC(self->sample_info_size)

        GETBYTES(self->sample_count, sample_info_size);
    }

    if (self->size > self->bytesRead) {
        u64 bytesToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);
    if (err) {
        if (self && self->sample_info_size) {
            MP4LocalFree(self->sample_info_size);
            self->sample_info_size = NULL;
        }
    }

    return err;
}

static MP4Err getSampleSize(MP4SampleAuxiliaryInfoSizeAtomPtr s, u32 sampleNumber, u32* outSize) {
    MP4Err err;
    MP4SampleAuxiliaryInfoSizeAtomPtr self = (MP4SampleAuxiliaryInfoSizeAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outSize == NULL) || (sampleNumber > self->sample_count))
        BAILWITHERROR(MP4BadParamErr)

    if (self->default_sample_info_size) /* samples are the same size */
    {
        *outSize = self->default_sample_info_size;
    } else {
        *outSize = (u32)self->sample_info_size[sampleNumber];
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSampleAuxiliaryInfoSizeAtom(MP4SampleAuxiliaryInfoSizeAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleAuxiliaryInfoSizeAtomPtr self;

    self = (MP4SampleAuxiliaryInfoSizeAtomPtr)MP4LocalCalloc(
            1, sizeof(MP4SampleAuxiliaryInfoSizeAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SampleAuxiliaryInfoSizeAtomType;
    self->name = "sample auxiliary info size";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getSampleSize = getSampleSize;

    self->aux_info_type = 0;
    self->aux_info_type_parameter = 0;
    self->default_sample_info_size = 0;
    self->sample_count = 0;
    self->sample_info_size = NULL;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
