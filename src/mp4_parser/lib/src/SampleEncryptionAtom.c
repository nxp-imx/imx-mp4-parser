/*
 ***********************************************************************
 * Copyright 2020, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleEncryptionAtomPtr self;
    err = MP4NoErr;
    self = (MP4SampleEncryptionAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4SampleEncryptionAtomPtr self = (MP4SampleEncryptionAtomPtr)s;
    u64 bytesToRead;
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(sample_count);

    self->bSubsampleEncryption = (self->flags & 0x2);

    if (self->size > self->bytesRead) {
        bytesToRead = self->size - self->bytesRead;
        MP4MSG("MP4SampleEncryptionAtom bytesToRead=%lld", (long long)bytesToRead);
        self->data = (u8*)MP4LocalCalloc(bytesToRead, sizeof(u8));
        TESTMALLOC(self->data)
        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getEncryptionSampleCount(MP4SampleEncryptionAtomPtr s, u32* sample_count) {
    MP4Err err;
    MP4SampleEncryptionAtomPtr self = (MP4SampleEncryptionAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (sample_count == NULL))
        BAILWITHERROR(MP4BadParamErr)

    *sample_count = self->sample_count;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSampleEncryptionAtom(MP4SampleEncryptionAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleEncryptionAtomPtr self;

    self = (MP4SampleEncryptionAtomPtr)MP4LocalCalloc(1, sizeof(MP4SampleEncryptionAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SampleEncryptionAtomType;
    self->name = "Sample Encryption Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getSampleCount = getEncryptionSampleCount;
    self->data = NULL;
    self->sample_count = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
