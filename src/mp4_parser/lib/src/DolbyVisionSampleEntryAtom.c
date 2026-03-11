
/*
 * Copyright 2022, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    DolbyVisionSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (DolbyVisionSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->AV1CAtomPtr) {
        self->AV1CAtomPtr->destroy(self->AV1CAtomPtr);
        self->AV1CAtomPtr = NULL;
    }
    if (self->DVVCAtomPtr) {
        self->DVVCAtomPtr->destroy(self->DVVCAtomPtr);
        self->DVVCAtomPtr = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    DolbyVisionSampleEntryAtomPtr self = (DolbyVisionSampleEntryAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GETBYTES(16, reserved2);
    GET16(video_width);
    GET16(video_height);
    GETBYTES(50, reserved3);

    MP4MSG("Dolby %d/%d\n", self->video_width, self->video_height);

    while (self->bytesRead + 8 < self->size) {
        MP4AtomPtr outAtom = NULL;
        err = MP4ParseAtom(inputStream, &outAtom);
        if (err)
            break;

        self->bytesRead += outAtom->size;

        if (MP4Av1CAtomType == outAtom->type && self->AV1CAtomPtr == NULL) {
            self->AV1CAtomPtr = outAtom;
        } else if (MP4dvvCAtomType == outAtom->type && self->DVVCAtomPtr == NULL) {
            self->DVVCAtomPtr = outAtom;
        } else {
            outAtom->destroy(outAtom);
            outAtom = NULL;
        }
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateDolbyVisionSampleEntryAtom(DolbyVisionSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    DolbyVisionSampleEntryAtomPtr self;

    self = (DolbyVisionSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(DolbyVisionSampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->name = "DolbyVision sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
