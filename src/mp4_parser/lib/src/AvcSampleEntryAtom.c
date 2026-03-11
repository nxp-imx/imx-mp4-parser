/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
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
    MP4AvcSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4AvcSampleEntryAtomPtr)s;
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
    MP4AvcSampleEntryAtomPtr self = (MP4AvcSampleEntryAtomPtr)s;

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

    /* engr96382:
    For QuickTime movie, in  visual sample entry 'avc1' atom,
    there may be optional atoms preceding the configuration 'avcC' atom.
    In other words, 'avcC' may not be the 1st child atom of 'avc1' sample entry.
    So we need to scan the child atoms until 'avcC' is found.
    - Amanda */

    // GETATOM(AVCCAtomPtr);
    while (self->bytesRead + 8 < self->size) {
        MP4AtomPtr outAtom = NULL;
        err = MP4ParseAtom(inputStream, &outAtom);
        if (err)
            break;

        self->bytesRead += outAtom->size;

        if (MP4AvccAtomType == outAtom->type &&
            self->AVCCAtomPtr == NULL) { /* the 'avCC' child atom is found */
            self->AVCCAtomPtr = outAtom;
        }
        // get first colr atom
        else if (QTColorParameterAtomType == outAtom->type && self->colrAtomPtr == NULL) {
            self->colrAtomPtr = outAtom;
        } else if (MP4dvvCAtomType == outAtom->type && self->DVVCAtomPtr == NULL) {
            self->DVVCAtomPtr = outAtom;
        } else {
            /* other atoms, such as 'colr'. Overlook it.*/
            self->skipped_size += outAtom->size;
            outAtom->destroy(outAtom);
            outAtom = NULL;
        }
    }

    /* ENGR95485:
    For QuickTime movie, in  visual sample entry 'avc1' atom,
    there may be optional atoms following the configuration 'avcC' atom.
    and the total size of these atoms may be less than  the remaining data size of 'avc1'.
    These atoms are not standard optional 'm4ds', but can be 'colr' 'fiel'.
    See QuickTime spec p100 "video sample description extensions"
    Overlook such atoms.
    - Amanda */

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        // printf("redundant atoms found, total size %d\n", redundance_size);

        SKIPBYTES_FORWARD(redundance_size);
        self->skipped_size += redundance_size;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateAvcSampleEntryAtom(MP4AvcSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4AvcSampleEntryAtomPtr self;

    self = (MP4AvcSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4AvcSampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4AvcSampleEntryAtomType;
    self->name = "AVC sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
