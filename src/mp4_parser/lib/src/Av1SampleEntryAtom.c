
/*
 * Copyright 2020, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4Av1SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4Av1SampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->AVCCAtomPtr) {
        self->AVCCAtomPtr->destroy(self->AVCCAtomPtr);
        self->AVCCAtomPtr = NULL;
    }
    if (self->colrAtomPtr) {
        self->colrAtomPtr->destroy(self->colrAtomPtr);
        self->colrAtomPtr = NULL;
    }
    if (self->BitrateAtomPtr) {
        self->BitrateAtomPtr->destroy(self->BitrateAtomPtr);
        self->BitrateAtomPtr = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4Av1SampleEntryAtomPtr self = (MP4Av1SampleEntryAtomPtr)s;

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

    GET32(reserved3);  // horizresolution = 0x00480000
    GET32(reserved4);  // vertresolution = 0x00480000
    GET32(reserved5);  // reserved = 0
    GET16(reserved6);  // frame count =1
    GET8(nameLength);
    GETBYTES(4, name4);
    GETBYTES(27, reserved7);
    GET32(reserved8);  // depth = 0x0018 & predefined = -1

    while (self->bytesRead + 8 < self->size) {
        MP4AtomPtr outAtom = NULL;
        err = MP4ParseAtom(inputStream, &outAtom);
        if (err)
            break;

        self->bytesRead += outAtom->size;

        if (MP4Av1CAtomType == outAtom->type &&
            self->AVCCAtomPtr == NULL) { /* the 'avCC' child atom is found */
            self->AVCCAtomPtr = outAtom;
        } else if (QTColorParameterAtomType == outAtom->type && self->colrAtomPtr == NULL) {
            self->colrAtomPtr = outAtom;
        } else if (MP4BitrateAtomType == outAtom->type && self->BitrateAtomPtr == NULL) {
            self->BitrateAtomPtr = outAtom;
        } else {
            self->skipped_size += outAtom->size;
            outAtom->destroy(outAtom);
            outAtom = NULL;
        }
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
        self->skipped_size += redundance_size;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateAv1SampleEntryAtom(MP4Av1SampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4Av1SampleEntryAtomPtr self;

    self = (MP4Av1SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4Av1SampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4Av1SampleEntryAtomType;
    self->name = "AV1 sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
