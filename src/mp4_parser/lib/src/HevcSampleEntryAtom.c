/*
 ***********************************************************************
 * Copyright 2014 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2022, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4HevcSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4HevcSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->HVCCAtomPtr) {
        self->HVCCAtomPtr->destroy(self->HVCCAtomPtr);
        self->HVCCAtomPtr = NULL;
    }
    if (self->colrAtomPtr) {
        self->colrAtomPtr->destroy(self->colrAtomPtr);
        self->colrAtomPtr = NULL;
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
    MP4HevcSampleEntryAtomPtr self = (MP4HevcSampleEntryAtomPtr)s;

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

        if (MP4HvccAtomType == outAtom->type &&
            self->HVCCAtomPtr == NULL)  // self->AVCCAtomPtr->type
        {                               /* the 'hvcC' child atom is found */
            self->HVCCAtomPtr = outAtom;
        }
        // get first colr atom
        else if (QTColorParameterAtomType == outAtom->type && self->colrAtomPtr == NULL) {
            self->colrAtomPtr = outAtom;
        } else if (MP4dvvCAtomType == outAtom->type && self->DVVCAtomPtr == NULL) {
            self->DVVCAtomPtr = outAtom;
        } else {                                  /* other atoms, such as 'colr'. Overlook it.*/
            self->skipped_size += outAtom->size;  // self->AVCCAtomPtr->size;
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

MP4Err MP4CreateHevcSampleEntryAtom(MP4HevcSampleEntryAtomPtr* outAtom, u32 atomType) {
    MP4Err err;
    MP4HevcSampleEntryAtomPtr self;

    self = (MP4HevcSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4HevcSampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = atomType;
    self->name = "HEVC sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
