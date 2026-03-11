
/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

// #define DEBUG
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4Vp9SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4Vp9SampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->VPCCAtomPtr) {
        MP4MSG("        Vp9SampleEntry destroy del VPCCAtom\n");
        self->VPCCAtomPtr->destroy(self->VPCCAtomPtr);
        self->VPCCAtomPtr = NULL;
    }
    if (self->PASPAtomPtr) {
        MP4MSG("        Vp9SampleEntry destroy del PASPAtom\n");
        self->PASPAtomPtr->destroy(self->PASPAtomPtr);
        self->PASPAtomPtr = NULL;
    }
    if (self->BitrateAtomPtr) {
        MP4MSG("        Vp9SampleEntry destroy del BitrateAtom\n");
        self->BitrateAtomPtr->destroy(self->BitrateAtomPtr);
        self->BitrateAtomPtr = NULL;
    }

    MP4MSG("        Vp9SampleEntry destroy del super\n");

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    MP4MSG("        Vp9SampleEntry destroy end\n");

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4Vp9SampleEntryAtomPtr self = (MP4Vp9SampleEntryAtomPtr)s;

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

        if (MP4VpccAtomType == outAtom->type &&
            self->VPCCAtomPtr == NULL) { /* the 'vpcC' child atom is found */
            self->VPCCAtomPtr = outAtom;
        } else if (MP4PixelAspectRatioAtomType == outAtom->type && self->PASPAtomPtr == NULL) {
            self->PASPAtomPtr = outAtom;
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

MP4Err MP4CreateVp9SampleEntryAtom(MP4Vp9SampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4Vp9SampleEntryAtomPtr self;

    self = (MP4Vp9SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4Vp9SampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4Vp9SampleEntryAtomType;
    self->name = "Vp9 sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
